/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "gtp/gtp-types.h"
#include "gtp/gtp-conv.h"
#include "gtp/gtp-message.h"
#include "gtp/gtp-node.h"

#include "mme-context.h"

#include "mme-s11-build.h"

int mme_s11_build_create_session_request(
        ogs_pkbuf_t **pkbuf, uint8_t type, mme_sess_t *sess)
{
    int rv;
    pdn_t *pdn = NULL;
    mme_ue_t *mme_ue = NULL;
    mme_bearer_t *bearer = NULL;
    gtp_message_t gtp_message;
    gtp_create_session_request_t *req = &gtp_message.create_session_request;

    gtp_uli_t uli;
    char uli_buf[GTP_MAX_ULI_LEN];
    gtp_f_teid_t mme_s11_teid, pgw_s5c_teid;
    int len;
    gtp_ambr_t ambr;
    gtp_bearer_qos_t bearer_qos;
    char bearer_qos_buf[GTP_BEARER_QOS_LEN];
    gtp_ue_timezone_t ue_timezone;
    struct timeval now;
    struct tm time_exp;
    char apn[MAX_APN_LEN];

    ogs_assert(sess);
    pdn = sess->pdn;
    ogs_assert(pdn);
    bearer = mme_default_bearer_in_sess(sess);
    ogs_assert(bearer);
    mme_ue = sess->mme_ue;
    ogs_assert(mme_ue);

    ogs_debug("[MME] Create Session Request");
    ogs_debug("    MME_S11_TEID[%d] SGW_S11_TEID[%d]",
            mme_ue->mme_s11_teid, mme_ue->sgw_s11_teid);
    memset(&gtp_message, 0, sizeof(gtp_message_t));

    ogs_assert(mme_ue->imsi_len);
    req->imsi.presence = 1;
    req->imsi.data = mme_ue->imsi;
    req->imsi.len = mme_ue->imsi_len;

    memset(&uli, 0, sizeof(gtp_uli_t));
    uli.flags.e_cgi = 1;
    uli.flags.tai = 1;
    memcpy(&uli.tai.plmn_id, &mme_ue->tai.plmn_id, sizeof(uli.tai.plmn_id));
    uli.tai.tac = mme_ue->tai.tac;
    memcpy(&uli.e_cgi.plmn_id, &mme_ue->e_cgi.plmn_id, 
            sizeof(uli.e_cgi.plmn_id));
    uli.e_cgi.cell_id = mme_ue->e_cgi.cell_id;
    req->user_location_information.presence = 1;
    gtp_build_uli(&req->user_location_information, &uli, 
            uli_buf, GTP_MAX_ULI_LEN);

    req->serving_network.presence = 1;
    req->serving_network.data = &mme_ue->tai.plmn_id;
    req->serving_network.len = sizeof(mme_ue->tai.plmn_id);

    req->rat_type.presence = 1;
    req->rat_type.u8 = GTP_RAT_TYPE_EUTRAN;

    memset(&mme_s11_teid, 0, sizeof(gtp_f_teid_t));
    mme_s11_teid.interface_type = GTP_F_TEID_S11_MME_GTP_C;
    mme_s11_teid.teid = htonl(mme_ue->mme_s11_teid);
    rv = gtp_sockaddr_to_f_teid(
            mme_self()->gtpc_addr, mme_self()->gtpc_addr6, &mme_s11_teid, &len);
    ogs_assert(rv == OGS_OK);
    req->sender_f_teid_for_control_plane.presence = 1;
    req->sender_f_teid_for_control_plane.data = &mme_s11_teid;
    req->sender_f_teid_for_control_plane.len = len;

    memset(&pgw_s5c_teid, 0, sizeof(gtp_f_teid_t));
    pgw_s5c_teid.interface_type = GTP_F_TEID_S5_S8_PGW_GTP_C;
    if (pdn->pgw_ip.ipv4 || pdn->pgw_ip.ipv6) {
        pgw_s5c_teid.ipv4 = pdn->pgw_ip.ipv4;
        pgw_s5c_teid.ipv6 = pdn->pgw_ip.ipv6;
        if (pgw_s5c_teid.ipv4 && pgw_s5c_teid.ipv6) {
            pgw_s5c_teid.both.addr = pdn->pgw_ip.both.addr;
            memcpy(pgw_s5c_teid.both.addr6, pdn->pgw_ip.both.addr6,
                    sizeof pdn->pgw_ip.both.addr6);
            req->pgw_s5_s8_address_for_control_plane_or_pmip.len =
                GTP_F_TEID_IPV4V6_LEN;
        } else if (pgw_s5c_teid.ipv4) {
            /* pdn->pgw_ip always uses both ip address memory */
            pgw_s5c_teid.addr = pdn->pgw_ip.both.addr;
            req->pgw_s5_s8_address_for_control_plane_or_pmip.len =
                GTP_F_TEID_IPV4_LEN;
        } else if (pgw_s5c_teid.ipv6) {
            /* pdn->pgw_ip always uses both ip address memory */
            memcpy(pgw_s5c_teid.addr6, pdn->pgw_ip.both.addr6,
                    sizeof pdn->pgw_ip.both.addr6);
            req->pgw_s5_s8_address_for_control_plane_or_pmip.len =
                GTP_F_TEID_IPV6_LEN;
        }
        req->pgw_s5_s8_address_for_control_plane_or_pmip.presence = 1;
        req->pgw_s5_s8_address_for_control_plane_or_pmip.data =
            &pgw_s5c_teid;
    } else {
        ogs_sockaddr_t *pgw_addr = NULL;
        ogs_sockaddr_t *pgw_addr6 = NULL;

        pgw_addr = mme_pgw_addr_find_by_apn(
                &mme_self()->pgw_list, AF_INET, pdn->apn);
        pgw_addr6 = mme_pgw_addr_find_by_apn(
                &mme_self()->pgw_list, AF_INET6, pdn->apn);
        if (!pgw_addr && !pgw_addr6) {
            pgw_addr = mme_self()->pgw_addr;
            pgw_addr6 = mme_self()->pgw_addr6;
        }

        rv = gtp_sockaddr_to_f_teid(pgw_addr, pgw_addr6, &pgw_s5c_teid, &len);
        ogs_assert(rv == OGS_OK);
        req->pgw_s5_s8_address_for_control_plane_or_pmip.presence = 1;
        req->pgw_s5_s8_address_for_control_plane_or_pmip.data = &pgw_s5c_teid;
        req->pgw_s5_s8_address_for_control_plane_or_pmip.len = len;
    }

    req->access_point_name.presence = 1;
    req->access_point_name.len = fqdn_build(apn, pdn->apn, strlen(pdn->apn));
    req->access_point_name.data = apn;

    req->selection_mode.presence = 1;
    req->selection_mode.u8 = 
        GTP_SELECTION_MODE_MS_OR_NETWORK_PROVIDED_APN | 0xfc;

    ogs_assert(sess->request_type.pdn_type ==
            NAS_PDN_CONNECTIVITY_PDN_TYPE_IPV4 ||
            sess->request_type.pdn_type ==
            NAS_PDN_CONNECTIVITY_PDN_TYPE_IPV6 ||
            sess->request_type.pdn_type ==
            NAS_PDN_CONNECTIVITY_PDN_TYPE_IPV4V6);
    if (pdn->pdn_type == HSS_PDN_TYPE_IPV4 ||
        pdn->pdn_type == HSS_PDN_TYPE_IPV6 ||
        pdn->pdn_type == HSS_PDN_TYPE_IPV4V6) {
        req->pdn_type.u8 = ((pdn->pdn_type + 1) & sess->request_type.pdn_type);
        ogs_assert(req->pdn_type.u8 != 0);
    } else if (pdn->pdn_type == HSS_PDN_TYPE_IPV4_OR_IPV6) {
        req->pdn_type.u8 = sess->request_type.pdn_type;
    } else
        ogs_assert_if_reached();
    req->pdn_type.presence = 1;

    pdn->paa.pdn_type = req->pdn_type.u8;
    req->pdn_address_allocation.data = &pdn->paa;
    if (req->pdn_type.u8 == GTP_PDN_TYPE_IPV4)
        req->pdn_address_allocation.len = PAA_IPV4_LEN;
    else if (req->pdn_type.u8 == GTP_PDN_TYPE_IPV6)
        req->pdn_address_allocation.len = PAA_IPV6_LEN;
    else if (req->pdn_type.u8 == GTP_PDN_TYPE_IPV4V6)
        req->pdn_address_allocation.len = PAA_IPV4V6_LEN;
    else
        ogs_assert_if_reached();
    req->pdn_address_allocation.presence = 1;

    req->maximum_apn_restriction.presence = 1;
    req->maximum_apn_restriction.u8 = GTP_APN_NO_RESTRICTION;

    if (pdn->ambr.uplink || pdn->ambr.downlink) {
        memset(&ambr, 0, sizeof(gtp_ambr_t));
        ambr.uplink = htonl(pdn->ambr.uplink);
        ambr.downlink = htonl(pdn->ambr.downlink);
        req->aggregate_maximum_bit_rate.presence = 1;
        req->aggregate_maximum_bit_rate.data = &ambr;
        req->aggregate_maximum_bit_rate.len = sizeof(ambr);
    }

    if (sess->ue_pco.length && sess->ue_pco.buffer) {
        req->protocol_configuration_options.presence = 1;
        req->protocol_configuration_options.data = sess->ue_pco.buffer;
        req->protocol_configuration_options.len = sess->ue_pco.length;
    }

    req->bearer_contexts_to_be_created.presence = 1;
    req->bearer_contexts_to_be_created.eps_bearer_id.presence = 1;
    req->bearer_contexts_to_be_created.eps_bearer_id.u8 = bearer->ebi;

    memset(&bearer_qos, 0, sizeof(bearer_qos));
    bearer_qos.qci = pdn->qos.qci;
    bearer_qos.priority_level = pdn->qos.arp.priority_level;
    bearer_qos.pre_emption_capability = pdn->qos.arp.pre_emption_capability;
    bearer_qos.pre_emption_vulnerability =
        pdn->qos.arp.pre_emption_vulnerability;
    req->bearer_contexts_to_be_created.bearer_level_qos.presence = 1;
    gtp_build_bearer_qos(&req->bearer_contexts_to_be_created.bearer_level_qos,
            &bearer_qos, bearer_qos_buf, GTP_BEARER_QOS_LEN);

    /* UE Time Zone */
    memset(&ue_timezone, 0, sizeof(ue_timezone));
    ogs_gettimeofday(&now);
    ogs_localtime(now.tv_sec, &time_exp);
    if (time_exp.tm_gmtoff >= 0) {
        ue_timezone.timezone = GTP_TIME_TO_BCD(time_exp.tm_gmtoff / 900);
    } else {
        ue_timezone.timezone = GTP_TIME_TO_BCD((-time_exp.tm_gmtoff) / 900);
        ue_timezone.timezone |= 0x08;
    }
    /* quarters of an hour */
    ue_timezone.daylight_saving_time = 
        GTP_UE_TIME_ZONE_NO_ADJUSTMENT_FOR_DAYLIGHT_SAVING_TIME;
    req->ue_time_zone.presence = 1;
    req->ue_time_zone.data = &ue_timezone;
    req->ue_time_zone.len = sizeof(ue_timezone);

    req->charging_characteristics.presence = 1;
    req->charging_characteristics.data = (uint8_t *)"\x54\x00";
    req->charging_characteristics.len = 2;

    gtp_message.h.type = type;
    rv = gtp_build_msg(pkbuf, &gtp_message);
    ogs_assert(rv == OGS_OK);

    return OGS_OK;
}

int mme_s11_build_modify_bearer_request(ogs_pkbuf_t **pkbuf,
        uint8_t type, mme_bearer_t *bearer, int uli_presence)
{
    int rv;
    gtp_message_t gtp_message;
    gtp_modify_bearer_request_t *req = &gtp_message.modify_bearer_request;

    gtp_f_teid_t enb_s1u_teid;
    int len;
    gtp_uli_t uli;
    char uli_buf[GTP_MAX_ULI_LEN];

    mme_ue_t *mme_ue = NULL;

    ogs_assert(bearer);
    mme_ue = bearer->mme_ue;
    ogs_assert(mme_ue);

    ogs_debug("[MME] Modifty Bearer Request");
    ogs_debug("    MME_S11_TEID[%d] SGW_S11_TEID[%d]",
            mme_ue->mme_s11_teid, mme_ue->sgw_s11_teid);
    ogs_debug("    ENB_S1U_TEID[%d] SGW_S1U_TEID[%d]",
        bearer->enb_s1u_teid, bearer->sgw_s1u_teid);

    memset(&gtp_message, 0, sizeof(gtp_message_t));

    /* Bearer Context : EBI */
    req->bearer_contexts_to_be_modified.presence = 1;
    req->bearer_contexts_to_be_modified.eps_bearer_id.presence = 1;
    req->bearer_contexts_to_be_modified.eps_bearer_id.u8 = bearer->ebi;

    /* Data Plane(DL) : ENB-S1U */
    memset(&enb_s1u_teid, 0, sizeof(gtp_f_teid_t));
    enb_s1u_teid.interface_type = GTP_F_TEID_S1_U_ENODEB_GTP_U;
    enb_s1u_teid.teid = htonl(bearer->enb_s1u_teid);
    rv = gtp_ip_to_f_teid(&bearer->enb_s1u_ip, &enb_s1u_teid, &len);
    ogs_assert(rv == OGS_OK);
    req->bearer_contexts_to_be_modified.s1_u_enodeb_f_teid.presence = 1;
    req->bearer_contexts_to_be_modified.s1_u_enodeb_f_teid.data = &enb_s1u_teid;
    req->bearer_contexts_to_be_modified.s1_u_enodeb_f_teid.len = len;

    if (uli_presence) {
        /* User Location Information(ULI) */
        memset(&uli, 0, sizeof(gtp_uli_t));
        uli.flags.e_cgi = 1;
        uli.flags.tai = 1;
        memcpy(&uli.tai.plmn_id, &mme_ue->tai.plmn_id, sizeof(uli.tai.plmn_id));
        uli.tai.tac = mme_ue->tai.tac;
        memcpy(&uli.e_cgi.plmn_id, &mme_ue->e_cgi.plmn_id, 
                sizeof(uli.e_cgi.plmn_id));
        uli.e_cgi.cell_id = mme_ue->e_cgi.cell_id;
        req->user_location_information.presence = 1;
        gtp_build_uli(&req->user_location_information, &uli, 
                uli_buf, GTP_MAX_ULI_LEN);
    }

    gtp_message.h.type = type;
    rv = gtp_build_msg(pkbuf, &gtp_message);
    ogs_assert(rv == OGS_OK);

    return OGS_OK;
}

int mme_s11_build_delete_session_request(
        ogs_pkbuf_t **pkbuf, uint8_t type, mme_sess_t *sess)
{
    int rv;
    gtp_message_t gtp_message;
    gtp_delete_session_request_t *req = &gtp_message.delete_session_request;

    gtp_uli_t uli;
    char uli_buf[GTP_MAX_ULI_LEN];
    gtp_indication_t indication;

    mme_bearer_t *bearer = NULL;
    mme_ue_t *mme_ue = NULL;

    ogs_assert(sess);
    mme_ue = sess->mme_ue;
    ogs_assert(mme_ue);
    bearer = mme_default_bearer_in_sess(sess);
    ogs_assert(bearer);

    ogs_debug("[MME] Delete Session Request");
    ogs_debug("    MME_S11_TEID[%d] SGW_S11_TEID[%d]",
            mme_ue->mme_s11_teid, mme_ue->sgw_s11_teid);

    memset(&gtp_message, 0, sizeof(gtp_message_t));

    req->linked_eps_bearer_id.presence = 1;
    req->linked_eps_bearer_id.u8 = bearer->ebi;

    /* User Location Information(ULI) */
    memset(&uli, 0, sizeof(gtp_uli_t));
    uli.flags.e_cgi = 1;
    uli.flags.tai = 1;
    memcpy(&uli.tai.plmn_id, &mme_ue->tai.plmn_id, sizeof(uli.tai.plmn_id));
    uli.tai.tac = mme_ue->tai.tac;
    memcpy(&uli.e_cgi.plmn_id, &mme_ue->e_cgi.plmn_id, sizeof(uli.tai.plmn_id));
    uli.e_cgi.cell_id = mme_ue->e_cgi.cell_id;
    req->user_location_information.presence = 1;
    gtp_build_uli(&req->user_location_information, &uli,
            uli_buf, GTP_MAX_ULI_LEN);

    memset(&indication, 0, sizeof(gtp_indication_t));
    indication.oi = 1;
    req->indication_flags.presence = 1;
    req->indication_flags.data = &indication;
    req->indication_flags.len = sizeof(gtp_indication_t);

    gtp_message.h.type = type;
    rv = gtp_build_msg(pkbuf, &gtp_message);
    ogs_assert(rv == OGS_OK);

    return OGS_OK;
}

int mme_s11_build_create_bearer_response(
        ogs_pkbuf_t **pkbuf, uint8_t type, mme_bearer_t *bearer)
{
    int rv;
    gtp_message_t gtp_message;
    gtp_create_bearer_response_t *rsp = &gtp_message.create_bearer_response;

    gtp_cause_t cause;
    gtp_f_teid_t enb_s1u_teid, sgw_s1u_teid;
    int len;
    gtp_uli_t uli;
    char uli_buf[GTP_MAX_ULI_LEN];
    gtp_ue_timezone_t ue_timezone;
    struct timeval now;
    struct tm time_exp;

    mme_ue_t *mme_ue = NULL;

    ogs_assert(bearer);
    mme_ue = bearer->mme_ue;
    ogs_assert(mme_ue);
    
    ogs_debug("[MME] Create Bearer Response");
    ogs_debug("    MME_S11_TEID[%d] SGW_S11_TEID[%d]",
            mme_ue->mme_s11_teid, mme_ue->sgw_s11_teid);

    memset(&gtp_message, 0, sizeof(gtp_message_t));

    /* Set Cause */
    memset(&cause, 0, sizeof(cause));
    cause.value = GTP_CAUSE_REQUEST_ACCEPTED;
    rsp->cause.presence = 1;
    rsp->cause.len = sizeof(cause);
    rsp->cause.data = &cause;

    /* Bearer Context : EBI */
    rsp->bearer_contexts.presence = 1;
    rsp->bearer_contexts.eps_bearer_id.presence = 1;
    rsp->bearer_contexts.eps_bearer_id.u8 = bearer->ebi;

    /* Data Plane(DL) : ENB-S1U */
    memset(&enb_s1u_teid, 0, sizeof(gtp_f_teid_t));
    enb_s1u_teid.interface_type = GTP_F_TEID_S1_U_ENODEB_GTP_U;
    enb_s1u_teid.teid = htonl(bearer->enb_s1u_teid);
    rv = gtp_ip_to_f_teid(&bearer->enb_s1u_ip, &enb_s1u_teid, &len);
    ogs_assert(rv == OGS_OK);
    rsp->bearer_contexts.s1_u_enodeb_f_teid.presence = 1;
    rsp->bearer_contexts.s1_u_enodeb_f_teid.data = &enb_s1u_teid;
    rsp->bearer_contexts.s1_u_enodeb_f_teid.len = len;
    
    /* Data Plane(UL) : SGW-S1U */
    memset(&sgw_s1u_teid, 0, sizeof(gtp_f_teid_t));
    sgw_s1u_teid.interface_type = GTP_F_TEID_S1_U_SGW_GTP_U;
    sgw_s1u_teid.teid = htonl(bearer->sgw_s1u_teid);
    rv = gtp_ip_to_f_teid(&bearer->sgw_s1u_ip, &sgw_s1u_teid, &len);
    ogs_assert(rv == OGS_OK);
    rsp->bearer_contexts.s4_u_sgsn_f_teid.presence = 1;
    rsp->bearer_contexts.s4_u_sgsn_f_teid.data = &sgw_s1u_teid;
    rsp->bearer_contexts.s4_u_sgsn_f_teid.len = GTP_F_TEID_IPV4_LEN;

    /* Bearer Context : Cause */
    rsp->bearer_contexts.cause.presence = 1;
    rsp->bearer_contexts.cause.len = sizeof(cause);
    rsp->bearer_contexts.cause.data = &cause;

    /* User Location Information(ULI) */
    memset(&uli, 0, sizeof(gtp_uli_t));
    uli.flags.e_cgi = 1;
    uli.flags.tai = 1;
    memcpy(&uli.tai.plmn_id, &mme_ue->tai.plmn_id, sizeof(uli.tai.plmn_id));
    uli.tai.tac = mme_ue->tai.tac;
    memcpy(&uli.e_cgi.plmn_id, &mme_ue->e_cgi.plmn_id, 
            sizeof(uli.e_cgi.plmn_id));
    uli.e_cgi.cell_id = mme_ue->e_cgi.cell_id;
    rsp->user_location_information.presence = 1;
    gtp_build_uli(&rsp->user_location_information, &uli, 
            uli_buf, GTP_MAX_ULI_LEN);

    /* UE Time Zone */
    memset(&ue_timezone, 0, sizeof(ue_timezone));
    ogs_gettimeofday(&now);
    ogs_localtime(now.tv_sec, &time_exp);
    if (time_exp.tm_gmtoff >= 0) {
        ue_timezone.timezone = GTP_TIME_TO_BCD(time_exp.tm_gmtoff / 900);
    } else {
        ue_timezone.timezone = GTP_TIME_TO_BCD((-time_exp.tm_gmtoff) / 900);
        ue_timezone.timezone |= 0x08;
    }
    ue_timezone.daylight_saving_time = 
        GTP_UE_TIME_ZONE_NO_ADJUSTMENT_FOR_DAYLIGHT_SAVING_TIME;
    rsp->ue_time_zone.presence = 1;
    rsp->ue_time_zone.data = &ue_timezone;
    rsp->ue_time_zone.len = sizeof(ue_timezone);

    gtp_message.h.type = type;
    rv = gtp_build_msg(pkbuf, &gtp_message);
    ogs_assert(rv == OGS_OK);

    return OGS_OK;
}

int mme_s11_build_update_bearer_response(
        ogs_pkbuf_t **pkbuf, uint8_t type, mme_bearer_t *bearer)
{
    int rv;
    gtp_message_t gtp_message;
    gtp_update_bearer_response_t *rsp = &gtp_message.update_bearer_response;

    gtp_cause_t cause;
    gtp_uli_t uli;
    char uli_buf[GTP_MAX_ULI_LEN];
    gtp_ue_timezone_t ue_timezone;
    struct timeval now;
    struct tm time_exp;

    mme_ue_t *mme_ue = NULL;

    ogs_assert(bearer);
    mme_ue = bearer->mme_ue;
    ogs_assert(mme_ue);

    ogs_debug("[MME] Update Bearer Response");
    ogs_debug("    MME_S11_TEID[%d] SGW_S11_TEID[%d]",
            mme_ue->mme_s11_teid, mme_ue->sgw_s11_teid);

    memset(&gtp_message, 0, sizeof(gtp_message_t));

    /* Set Cause */
    memset(&cause, 0, sizeof(cause));
    cause.value = GTP_CAUSE_REQUEST_ACCEPTED;
    rsp->cause.presence = 1;
    rsp->cause.len = sizeof(cause);
    rsp->cause.data = &cause;

    /* Bearer Context : EBI */
    rsp->bearer_contexts.presence = 1;
    rsp->bearer_contexts.eps_bearer_id.presence = 1;
    rsp->bearer_contexts.eps_bearer_id.u8 = bearer->ebi;

    /* Bearer Context : Cause */
    rsp->bearer_contexts.cause.presence = 1;
    rsp->bearer_contexts.cause.len = sizeof(cause);
    rsp->bearer_contexts.cause.data = &cause;

    /* User Location Information(ULI) */
    memset(&uli, 0, sizeof(gtp_uli_t));
    uli.flags.e_cgi = 1;
    uli.flags.tai = 1;
    memcpy(&uli.tai.plmn_id, &mme_ue->tai.plmn_id, sizeof(uli.tai.plmn_id));
    uli.tai.tac = mme_ue->tai.tac;
    memcpy(&uli.e_cgi.plmn_id, &mme_ue->e_cgi.plmn_id, 
            sizeof(uli.e_cgi.plmn_id));
    uli.e_cgi.cell_id = mme_ue->e_cgi.cell_id;
    rsp->user_location_information.presence = 1;
    gtp_build_uli(&rsp->user_location_information, &uli, 
            uli_buf, GTP_MAX_ULI_LEN);

    /* UE Time Zone */
    memset(&ue_timezone, 0, sizeof(ue_timezone));
    ogs_gettimeofday(&now);
    ogs_localtime(now.tv_sec, &time_exp);
    if (time_exp.tm_gmtoff >= 0) {
        ue_timezone.timezone = GTP_TIME_TO_BCD(time_exp.tm_gmtoff / 900);
    } else {
        ue_timezone.timezone = GTP_TIME_TO_BCD((-time_exp.tm_gmtoff) / 900);
        ue_timezone.timezone |= 0x08;
    }
    ue_timezone.daylight_saving_time = 
        GTP_UE_TIME_ZONE_NO_ADJUSTMENT_FOR_DAYLIGHT_SAVING_TIME;
    rsp->ue_time_zone.presence = 1;
    rsp->ue_time_zone.data = &ue_timezone;
    rsp->ue_time_zone.len = sizeof(ue_timezone);

    gtp_message.h.type = type;
    rv = gtp_build_msg(pkbuf, &gtp_message);
    ogs_assert(rv == OGS_OK);

    return OGS_OK;
}

int mme_s11_build_delete_bearer_response(
        ogs_pkbuf_t **pkbuf, uint8_t type, mme_bearer_t *bearer)
{
    int rv;
    gtp_message_t gtp_message;
    gtp_delete_bearer_response_t *rsp = &gtp_message.delete_bearer_response;

    gtp_cause_t cause;
    gtp_uli_t uli;
    char uli_buf[GTP_MAX_ULI_LEN];
    gtp_ue_timezone_t ue_timezone;
    struct timeval now;
    struct tm time_exp;

    mme_ue_t *mme_ue = NULL;

    ogs_assert(bearer);
    mme_ue = bearer->mme_ue;
    ogs_assert(mme_ue);

    ogs_debug("[MME] Delete Bearer Response");
    ogs_debug("    MME_S11_TEID[%d] SGW_S11_TEID[%d]",
            mme_ue->mme_s11_teid, mme_ue->sgw_s11_teid);

    memset(&gtp_message, 0, sizeof(gtp_message_t));

    /* Set Cause */
    memset(&cause, 0, sizeof(cause));
    cause.value = GTP_CAUSE_REQUEST_ACCEPTED;
    rsp->cause.presence = 1;
    rsp->cause.len = sizeof(cause);
    rsp->cause.data = &cause;

    /* Bearer Context : EBI */
    rsp->bearer_contexts.presence = 1;
    rsp->bearer_contexts.eps_bearer_id.presence = 1;
    rsp->bearer_contexts.eps_bearer_id.u8 = bearer->ebi;

    /* Bearer Context : Cause */
    rsp->bearer_contexts.cause.presence = 1;
    rsp->bearer_contexts.cause.len = sizeof(cause);
    rsp->bearer_contexts.cause.data = &cause;

    /* User Location Information(ULI) */
    memset(&uli, 0, sizeof(gtp_uli_t));
    uli.flags.e_cgi = 1;
    uli.flags.tai = 1;
    memcpy(&uli.tai.plmn_id, &mme_ue->tai.plmn_id, sizeof(uli.tai.plmn_id));
    uli.tai.tac = mme_ue->tai.tac;
    memcpy(&uli.e_cgi.plmn_id, &mme_ue->e_cgi.plmn_id, 
            sizeof(uli.e_cgi.plmn_id));
    uli.e_cgi.cell_id = mme_ue->e_cgi.cell_id;
    rsp->user_location_information.presence = 1;
    gtp_build_uli(&rsp->user_location_information, &uli, 
            uli_buf, GTP_MAX_ULI_LEN);

    /* UE Time Zone */
    memset(&ue_timezone, 0, sizeof(ue_timezone));
    ogs_gettimeofday(&now);
    ogs_localtime(now.tv_sec, &time_exp);
    if (time_exp.tm_gmtoff >= 0) {
        ue_timezone.timezone = GTP_TIME_TO_BCD(time_exp.tm_gmtoff / 900);
    } else {
        ue_timezone.timezone = GTP_TIME_TO_BCD((-time_exp.tm_gmtoff) / 900);
        ue_timezone.timezone |= 0x08;
    }
    ue_timezone.daylight_saving_time = 
        GTP_UE_TIME_ZONE_NO_ADJUSTMENT_FOR_DAYLIGHT_SAVING_TIME;
    rsp->ue_time_zone.presence = 1;
    rsp->ue_time_zone.data = &ue_timezone;
    rsp->ue_time_zone.len = sizeof(ue_timezone);

    gtp_message.h.type = type;
    rv = gtp_build_msg(pkbuf, &gtp_message);
    ogs_assert(rv == OGS_OK);

    return OGS_OK;
}

int mme_s11_build_release_access_bearers_request(
        ogs_pkbuf_t **pkbuf, uint8_t type)
{
    int rv;
    gtp_message_t gtp_message;
    gtp_release_access_bearers_request_t *req = 
        &gtp_message.release_access_bearers_request;

    ogs_debug("[MME] Release Access Bearers Request");
    memset(&gtp_message, 0, sizeof(gtp_message_t));

    req->originating_node.presence = 1;
    req->originating_node.u8 = GTP_NODE_TYPE_MME;

    gtp_message.h.type = type;
    rv = gtp_build_msg(pkbuf, &gtp_message);
    ogs_assert(rv == OGS_OK);

    return OGS_OK;
}

int mme_s11_build_downlink_data_notification_ack(
        ogs_pkbuf_t **pkbuf, uint8_t type)
{
    int rv;
    gtp_message_t gtp_message;
    gtp_downlink_data_notification_acknowledge_t *ack = 
        &gtp_message.downlink_data_notification_acknowledge;

    gtp_cause_t cause;

    ogs_debug("[MME] Downlink Data Notification Ackknowledge");

    memset(&gtp_message, 0, sizeof(gtp_message_t));

    memset(&cause, 0, sizeof(cause));
    cause.value = GTP_CAUSE_REQUEST_ACCEPTED;

    ack->cause.presence = 1;
    ack->cause.data = &cause;
    ack->cause.len = sizeof(cause);

    gtp_message.h.type = type;
    rv = gtp_build_msg(pkbuf, &gtp_message);
    ogs_assert(rv == OGS_OK);

    return OGS_OK;
}

int mme_s11_build_create_indirect_data_forwarding_tunnel_request(
        ogs_pkbuf_t **pkbuf, uint8_t type, mme_ue_t *mme_ue)
{
    int rv;
    int i;
    
    mme_sess_t *sess = NULL;
    mme_bearer_t *bearer = NULL;

    gtp_message_t gtp_message;
    gtp_create_indirect_data_forwarding_tunnel_request_t *req =
        &gtp_message.create_indirect_data_forwarding_tunnel_request;
    
    tlv_bearer_context_t *bearers[GTP_MAX_NUM_OF_INDIRECT_TUNNEL];
    gtp_f_teid_t dl_teid[GTP_MAX_NUM_OF_INDIRECT_TUNNEL];
    gtp_f_teid_t ul_teid[GTP_MAX_NUM_OF_INDIRECT_TUNNEL];
    int len;

    ogs_assert(mme_ue);

    ogs_debug("[MME] Create Indirect Data Forwarding Tunnel Request");
    ogs_debug("    MME_S11_TEID[%d] SGW_S11_TEID[%d]",
            mme_ue->mme_s11_teid, mme_ue->sgw_s11_teid);

    gtp_bearers_in_create_indirect_tunnel_request(&bearers, req);
    memset(&gtp_message, 0, sizeof(gtp_message_t));

    i = 0;
    sess = mme_sess_first(mme_ue);
    while (sess != NULL) {
        bearer = mme_bearer_first(sess);
        while (bearer != NULL) {
            if (MME_HAVE_ENB_DL_INDIRECT_TUNNEL(bearer)) {
                memset(&dl_teid[i], 0, sizeof(gtp_f_teid_t));
                dl_teid[i].interface_type =
                    GTP_F_TEID_ENODEB_GTP_U_FOR_DL_DATA_FORWARDING;
                dl_teid[i].teid = htonl(bearer->enb_dl_teid);
                rv = gtp_ip_to_f_teid(&bearer->enb_dl_ip, &dl_teid[i], &len);
                ogs_assert(rv == OGS_OK);
                ogs_assert(bearers[i]);
                bearers[i]->s1_u_enodeb_f_teid.presence = 1;
                bearers[i]->s1_u_enodeb_f_teid.data = &dl_teid[i];
                bearers[i]->s1_u_enodeb_f_teid.len = len;
            }

            if (MME_HAVE_ENB_UL_INDIRECT_TUNNEL(bearer)) {
                memset(&ul_teid[i], 0, sizeof(gtp_f_teid_t));
                ul_teid[i].interface_type =
                    GTP_F_TEID_ENODEB_GTP_U_FOR_UL_DATA_FORWARDING;
                ul_teid[i].teid = htonl(bearer->enb_ul_teid);
                rv = gtp_ip_to_f_teid(&bearer->enb_ul_ip, &ul_teid[i], &len);
                ogs_assert(rv == OGS_OK);
                ogs_assert(bearers[i]);
                bearers[i]->s12_rnc_f_teid.presence = 1;
                bearers[i]->s12_rnc_f_teid.data = &ul_teid[i];
                bearers[i]->s12_rnc_f_teid.len = len;
            }

            if (MME_HAVE_ENB_DL_INDIRECT_TUNNEL(bearer) ||
                MME_HAVE_ENB_UL_INDIRECT_TUNNEL(bearer)) {
                bearers[i]->presence = 1;
                bearers[i]->eps_bearer_id.presence = 1;
                bearers[i]->eps_bearer_id.u8 = bearer->ebi;
                i++;
            }

            bearer = mme_bearer_next(bearer);
        }
        sess = mme_sess_next(sess);
    }

    gtp_message.h.type = type;
    rv = gtp_build_msg(pkbuf, &gtp_message);
    ogs_assert(rv == OGS_OK);

    return OGS_OK;
}
