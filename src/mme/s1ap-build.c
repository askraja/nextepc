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

#include "fd/s6a/s6a-message.h"
#include "mme-context.h"

#include "mme-kdf.h"
#include "mme-sm.h"
#include "s1ap-build.h"
#include "s1ap-conv.h"

int s1ap_build_setup_rsp(ogs_pkbuf_t **pkbuf)
{
    int rv;
    int i, j;

    S1AP_S1AP_PDU_t pdu;
    S1AP_SuccessfulOutcome_t *successfulOutcome = NULL;
    S1AP_S1SetupResponse_t *S1SetupResponse = NULL;

    S1AP_S1SetupResponseIEs_t *ie = NULL;
    S1AP_ServedGUMMEIs_t *ServedGUMMEIs = NULL;
    S1AP_RelativeMMECapacity_t *RelativeMMECapacity = NULL;

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_successfulOutcome;
    pdu.choice.successfulOutcome = 
        CALLOC(1, sizeof(S1AP_SuccessfulOutcome_t));

    successfulOutcome = pdu.choice.successfulOutcome;
    successfulOutcome->procedureCode = S1AP_ProcedureCode_id_S1Setup;
    successfulOutcome->criticality = S1AP_Criticality_reject;
    successfulOutcome->value.present =
        S1AP_SuccessfulOutcome__value_PR_S1SetupResponse;

    S1SetupResponse = &successfulOutcome->value.choice.S1SetupResponse;

    ie = CALLOC(1, sizeof(S1AP_S1SetupResponseIEs_t));
    ASN_SEQUENCE_ADD(&S1SetupResponse->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_ServedGUMMEIs;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_S1SetupResponseIEs__value_PR_ServedGUMMEIs;

    ServedGUMMEIs = &ie->value.choice.ServedGUMMEIs;

    ie = CALLOC(1, sizeof(S1AP_S1SetupResponseIEs_t));
    ASN_SEQUENCE_ADD(&S1SetupResponse->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_RelativeMMECapacity;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_S1SetupResponseIEs__value_PR_RelativeMMECapacity;

    RelativeMMECapacity = &ie->value.choice.RelativeMMECapacity;

    for (i = 0; i < mme_self()->max_num_of_served_gummei; i++) {
        S1AP_ServedGUMMEIsItem_t *ServedGUMMEIsItem = NULL;
        ServedGUMMEIsItem = (S1AP_ServedGUMMEIsItem_t *)
            CALLOC(1, sizeof(S1AP_ServedGUMMEIsItem_t));

        served_gummei_t *served_gummei = &mme_self()->served_gummei[i];
        for (j = 0; j < served_gummei->num_of_plmn_id; j++) {
            S1AP_PLMNidentity_t *PLMNidentity = NULL;
            PLMNidentity = (S1AP_PLMNidentity_t *)
                CALLOC(1, sizeof(S1AP_PLMNidentity_t));
            s1ap_buffer_to_OCTET_STRING(
                    &served_gummei->plmn_id[j], PLMN_ID_LEN, PLMNidentity);
            ASN_SEQUENCE_ADD(
                    &ServedGUMMEIsItem->servedPLMNs.list, PLMNidentity);
            ogs_debug("    PLMN_ID[MCC:%d MNC:%d]",
                plmn_id_mcc(&served_gummei->plmn_id[j]),
                plmn_id_mnc(&served_gummei->plmn_id[j]));
        }

        for (j = 0; j < served_gummei->num_of_mme_gid; j++) {
            S1AP_MME_Group_ID_t *MME_Group_ID = NULL;
            MME_Group_ID = (S1AP_MME_Group_ID_t *)
                CALLOC(1, sizeof(S1AP_MME_Group_ID_t));
            s1ap_uint16_to_OCTET_STRING(
                    served_gummei->mme_gid[j], MME_Group_ID);
            ASN_SEQUENCE_ADD(
                    &ServedGUMMEIsItem->servedGroupIDs.list, MME_Group_ID);
            ogs_debug("    MME Group[%d]", served_gummei->mme_gid[j]);
        }

        for (j = 0; j < served_gummei->num_of_mme_code; j++) {
            S1AP_MME_Code_t *MME_Code = NULL ;
            MME_Code = (S1AP_MME_Code_t *)
                CALLOC(1, sizeof(S1AP_MME_Code_t));
            s1ap_uint8_to_OCTET_STRING(served_gummei->mme_code[j], MME_Code);
            ASN_SEQUENCE_ADD(&ServedGUMMEIsItem->servedMMECs.list, MME_Code);
            ogs_debug("    MME Code[%d]", served_gummei->mme_code[j]);
        }
        ASN_SEQUENCE_ADD(&ServedGUMMEIs->list, ServedGUMMEIsItem);
    }

    *RelativeMMECapacity = mme_self()->relative_capacity;

    rv = s1ap_encode_pdu(pkbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_setup_failure(
        ogs_pkbuf_t **pkbuf, S1AP_Cause_PR group, long cause, long time_to_wait)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_UnsuccessfulOutcome_t *unsuccessfulOutcome = NULL;
    S1AP_S1SetupFailure_t *S1SetupFailure = NULL;

    S1AP_S1SetupFailureIEs_t *ie = NULL;
    S1AP_Cause_t *Cause = NULL;
    S1AP_TimeToWait_t *TimeToWait = NULL;
    
    ogs_debug("    Group[%d] Cause[%d] TimeToWait[%ld]",
            group, (int)cause, time_to_wait);

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_unsuccessfulOutcome;
    pdu.choice.unsuccessfulOutcome = 
        CALLOC(1, sizeof(S1AP_UnsuccessfulOutcome_t));

    unsuccessfulOutcome = pdu.choice.unsuccessfulOutcome;
    unsuccessfulOutcome->procedureCode = S1AP_ProcedureCode_id_S1Setup;
    unsuccessfulOutcome->criticality = S1AP_Criticality_reject;
    unsuccessfulOutcome->value.present =
        S1AP_UnsuccessfulOutcome__value_PR_S1SetupFailure;

    S1SetupFailure = &unsuccessfulOutcome->value.choice.S1SetupFailure;

    ie = CALLOC(1, sizeof(S1AP_S1SetupFailureIEs_t));
    ASN_SEQUENCE_ADD(&S1SetupFailure->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_Cause;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_S1SetupFailureIEs__value_PR_Cause;

    Cause = &ie->value.choice.Cause;

    if (time_to_wait > -1) {
        ie = CALLOC(1, sizeof(S1AP_S1SetupFailureIEs_t));
        ASN_SEQUENCE_ADD(&S1SetupFailure->protocolIEs, ie);

        ie->id = S1AP_ProtocolIE_ID_id_TimeToWait;
        ie->criticality = S1AP_Criticality_ignore;
        ie->value.present = S1AP_S1SetupFailureIEs__value_PR_TimeToWait;

        TimeToWait = &ie->value.choice.TimeToWait;
    }

    Cause->present = group;
    Cause->choice.radioNetwork = cause;

    if (TimeToWait)
        *TimeToWait = time_to_wait;

    rv = s1ap_encode_pdu(pkbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_downlink_nas_transport(
            ogs_pkbuf_t **s1apbuf, enb_ue_t *enb_ue, ogs_pkbuf_t *emmbuf)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_DownlinkNASTransport_t *DownlinkNASTransport = NULL;

    S1AP_DownlinkNASTransport_IEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_NAS_PDU_t *NAS_PDU = NULL;

    ogs_assert(emmbuf);
    ogs_assert(enb_ue);

    ogs_debug("[MME] DownlinkNASTransport");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode =
        S1AP_ProcedureCode_id_downlinkNASTransport;
    initiatingMessage->criticality = S1AP_Criticality_ignore;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_DownlinkNASTransport;

    DownlinkNASTransport =
        &initiatingMessage->value.choice.DownlinkNASTransport;

    ie = CALLOC(1, sizeof(S1AP_DownlinkNASTransport_IEs_t));
    ASN_SEQUENCE_ADD(&DownlinkNASTransport->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_DownlinkNASTransport_IEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_DownlinkNASTransport_IEs_t));
    ASN_SEQUENCE_ADD(&DownlinkNASTransport->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_DownlinkNASTransport_IEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_DownlinkNASTransport_IEs_t));
    ASN_SEQUENCE_ADD(&DownlinkNASTransport->protocolIEs, ie);
    
    ie->id = S1AP_ProtocolIE_ID_id_NAS_PDU;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_DownlinkNASTransport_IEs__value_PR_NAS_PDU;

    NAS_PDU = &ie->value.choice.NAS_PDU;

    ogs_debug("    ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            enb_ue->enb_ue_s1ap_id, enb_ue->mme_ue_s1ap_id);

    *MME_UE_S1AP_ID = enb_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = enb_ue->enb_ue_s1ap_id;

    NAS_PDU->size = emmbuf->len;
    NAS_PDU->buf = CALLOC(NAS_PDU->size, sizeof(uint8_t));
    memcpy(NAS_PDU->buf, emmbuf->data, NAS_PDU->size);
    ogs_pkbuf_free(emmbuf);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_initial_context_setup_request(
            ogs_pkbuf_t **s1apbuf, mme_ue_t *mme_ue, ogs_pkbuf_t *emmbuf)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_InitialContextSetupRequest_t *InitialContextSetupRequest = NULL;

    S1AP_InitialContextSetupRequestIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_UEAggregateMaximumBitrate_t *UEAggregateMaximumBitrate = NULL;
    S1AP_E_RABToBeSetupListCtxtSUReq_t *E_RABToBeSetupListCtxtSUReq = NULL;
    S1AP_UESecurityCapabilities_t *UESecurityCapabilities = NULL;
    S1AP_SecurityKey_t *SecurityKey = NULL;

    enb_ue_t *enb_ue = NULL;
    mme_sess_t *sess = NULL;
    mme_bearer_t *bearer = NULL;
    s6a_subscription_data_t *subscription_data = NULL;

    ogs_assert(mme_ue);
    enb_ue = mme_ue->enb_ue;
    ogs_assert(enb_ue);
    subscription_data = &mme_ue->subscription_data;
    ogs_assert(subscription_data);

    ogs_debug("[MME] Initial context setup request");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode =
        S1AP_ProcedureCode_id_InitialContextSetup;
    initiatingMessage->criticality = S1AP_Criticality_reject;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_InitialContextSetupRequest;

    InitialContextSetupRequest =
        &initiatingMessage->value.choice.InitialContextSetupRequest;

    ie = CALLOC(1, sizeof(S1AP_InitialContextSetupRequestIEs_t));
    ASN_SEQUENCE_ADD(&InitialContextSetupRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_InitialContextSetupRequestIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_InitialContextSetupRequestIEs_t));
    ASN_SEQUENCE_ADD(&InitialContextSetupRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_InitialContextSetupRequestIEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_InitialContextSetupRequestIEs_t));
    ASN_SEQUENCE_ADD(&InitialContextSetupRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_uEaggregateMaximumBitrate;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_InitialContextSetupRequestIEs__value_PR_UEAggregateMaximumBitrate;

    UEAggregateMaximumBitrate = &ie->value.choice.UEAggregateMaximumBitrate;

    ie = CALLOC(1, sizeof(S1AP_InitialContextSetupRequestIEs_t));
    ASN_SEQUENCE_ADD(&InitialContextSetupRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_E_RABToBeSetupListCtxtSUReq;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
    S1AP_InitialContextSetupRequestIEs__value_PR_E_RABToBeSetupListCtxtSUReq;

    E_RABToBeSetupListCtxtSUReq = &ie->value.choice.E_RABToBeSetupListCtxtSUReq;

    ogs_debug("    ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            enb_ue->enb_ue_s1ap_id, enb_ue->mme_ue_s1ap_id);

    *MME_UE_S1AP_ID = enb_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = enb_ue->enb_ue_s1ap_id;

    asn_uint642INTEGER(
            &UEAggregateMaximumBitrate->uEaggregateMaximumBitRateUL, 
            subscription_data->ambr.uplink);
    asn_uint642INTEGER(
            &UEAggregateMaximumBitrate->uEaggregateMaximumBitRateDL, 
            subscription_data->ambr.downlink);

    sess = mme_sess_first(mme_ue);
    while (sess) {
        bearer = mme_bearer_first(sess);
        while (bearer) {
            S1AP_E_RABToBeSetupItemCtxtSUReqIEs_t *item = NULL;
            S1AP_E_RABToBeSetupItemCtxtSUReq_t *e_rab = NULL;
            S1AP_GBR_QosInformation_t *gbrQosInformation = NULL;
            S1AP_NAS_PDU_t *nasPdu = NULL;

            item = CALLOC(
                    1, sizeof(S1AP_E_RABToBeSetupItemCtxtSUReqIEs_t));
            ASN_SEQUENCE_ADD(&E_RABToBeSetupListCtxtSUReq->list, item);

            item->id = S1AP_ProtocolIE_ID_id_E_RABToBeSetupItemCtxtSUReq;
            item->criticality = S1AP_Criticality_reject;
            item->value.present = S1AP_E_RABToBeSetupItemCtxtSUReqIEs__value_PR_E_RABToBeSetupItemCtxtSUReq;

            e_rab = &item->value.choice.E_RABToBeSetupItemCtxtSUReq;

            e_rab->e_RAB_ID = bearer->ebi;
            e_rab->e_RABlevelQoSParameters.qCI = bearer->qos.qci;

            ogs_debug("    EBI[%d] QCI[%d] SGW-S1U-TEID[%d]",
                    bearer->ebi, bearer->qos.qci, bearer->sgw_s1u_teid);

            e_rab->e_RABlevelQoSParameters.allocationRetentionPriority.
                priorityLevel = bearer->qos.arp.priority_level;
            e_rab->e_RABlevelQoSParameters.allocationRetentionPriority.
                pre_emptionCapability =
                    !(bearer->qos.arp.pre_emption_capability);
            e_rab->e_RABlevelQoSParameters.allocationRetentionPriority.
                pre_emptionVulnerability =
                    !(bearer->qos.arp.pre_emption_vulnerability);

            if (bearer->qos.mbr.downlink || bearer->qos.mbr.uplink ||
                bearer->qos.gbr.downlink || bearer->qos.gbr.uplink) {
                if (bearer->qos.mbr.downlink == 0)
                    bearer->qos.mbr.downlink = MAX_BIT_RATE;
                if (bearer->qos.mbr.uplink == 0)
                    bearer->qos.mbr.uplink = MAX_BIT_RATE;
                if (bearer->qos.gbr.downlink == 0)
                    bearer->qos.gbr.downlink = MAX_BIT_RATE;
                if (bearer->qos.gbr.uplink == 0)
                    bearer->qos.gbr.uplink = MAX_BIT_RATE;

                gbrQosInformation = 
                        CALLOC(1, sizeof(struct S1AP_GBR_QosInformation));
                asn_uint642INTEGER(&gbrQosInformation->e_RAB_MaximumBitrateDL,
                        bearer->qos.mbr.downlink);
                asn_uint642INTEGER(&gbrQosInformation->e_RAB_MaximumBitrateUL,
                        bearer->qos.mbr.uplink);
                asn_uint642INTEGER(&gbrQosInformation->
                        e_RAB_GuaranteedBitrateDL, bearer->qos.gbr.downlink);
                asn_uint642INTEGER(&gbrQosInformation->
                        e_RAB_GuaranteedBitrateUL, bearer->qos.gbr.uplink);
                e_rab->e_RABlevelQoSParameters.gbrQosInformation =
                        gbrQosInformation;
            }

            rv = s1ap_ip_to_BIT_STRING(
                    &bearer->sgw_s1u_ip, &e_rab->transportLayerAddress);
            ogs_assert(rv == OGS_OK);
            s1ap_uint32_to_OCTET_STRING(bearer->sgw_s1u_teid, &e_rab->gTP_TEID);

            if (emmbuf && emmbuf->len) {
                nasPdu = (S1AP_NAS_PDU_t *)CALLOC(
                        1, sizeof(S1AP_NAS_PDU_t));
                nasPdu->size = emmbuf->len;
                nasPdu->buf = CALLOC(nasPdu->size, sizeof(uint8_t));
                memcpy(nasPdu->buf, emmbuf->data, nasPdu->size);
                e_rab->nAS_PDU = nasPdu;
                ogs_pkbuf_free(emmbuf);
            }

            bearer = mme_bearer_next(bearer);
        }
        sess = mme_sess_next(sess);
    }

    ie = CALLOC(1, sizeof(S1AP_InitialContextSetupRequestIEs_t));
    ASN_SEQUENCE_ADD(&InitialContextSetupRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_UESecurityCapabilities;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_InitialContextSetupRequestIEs__value_PR_UESecurityCapabilities;

    UESecurityCapabilities = &ie->value.choice.UESecurityCapabilities;

    UESecurityCapabilities->encryptionAlgorithms.size = 2;
    UESecurityCapabilities->encryptionAlgorithms.buf = 
        CALLOC(UESecurityCapabilities->encryptionAlgorithms.size, 
                    sizeof(uint8_t));
    UESecurityCapabilities->encryptionAlgorithms.bits_unused = 0;
    UESecurityCapabilities->encryptionAlgorithms.buf[0] = 
        (mme_ue->ue_network_capability.eea << 1);

    UESecurityCapabilities->integrityProtectionAlgorithms.size = 2;
    UESecurityCapabilities->integrityProtectionAlgorithms.buf =
        CALLOC(UESecurityCapabilities->
                        integrityProtectionAlgorithms.size, sizeof(uint8_t));
    UESecurityCapabilities->integrityProtectionAlgorithms.bits_unused = 0;
    UESecurityCapabilities->integrityProtectionAlgorithms.buf[0] =
        (mme_ue->ue_network_capability.eia << 1);

    ie = CALLOC(1, sizeof(S1AP_InitialContextSetupRequestIEs_t));
    ASN_SEQUENCE_ADD(&InitialContextSetupRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_SecurityKey;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_InitialContextSetupRequestIEs__value_PR_SecurityKey;

    SecurityKey = &ie->value.choice.SecurityKey;

    SecurityKey->size = OGS_SHA256_DIGEST_SIZE;
    SecurityKey->buf = 
        CALLOC(SecurityKey->size, sizeof(uint8_t));
    SecurityKey->bits_unused = 0;
    memcpy(SecurityKey->buf, mme_ue->kenb, SecurityKey->size);

    /* Workaround for asn1c library
     * - More than 9 ProtocolIE cannot be built 
     *   from InitialContextSetupRequest */
    if (mme_ue->nas_eps.type == MME_EPS_TYPE_EXTENDED_SERVICE_REQUEST &&
        MME_P_TMSI_IS_AVAILABLE(mme_ue)) {

        /* Set CS-Fallback */
        S1AP_CSFallbackIndicator_t *CSFallbackIndicator = NULL;
        S1AP_LAI_t *LAI = NULL;

        ie = CALLOC(1, sizeof(S1AP_InitialContextSetupRequestIEs_t));
        ASN_SEQUENCE_ADD(&InitialContextSetupRequest->protocolIEs, ie);

        ie->id = S1AP_ProtocolIE_ID_id_CSFallbackIndicator;
        ie->criticality = S1AP_Criticality_reject;
        ie->value.present =
            S1AP_InitialContextSetupRequestIEs__value_PR_CSFallbackIndicator;

        CSFallbackIndicator = &ie->value.choice.CSFallbackIndicator;
        ogs_assert(CSFallbackIndicator);

        *CSFallbackIndicator = S1AP_CSFallbackIndicator_cs_fallback_required;

        ie = CALLOC(1, sizeof(S1AP_InitialContextSetupRequestIEs_t));
        ASN_SEQUENCE_ADD(&InitialContextSetupRequest->protocolIEs, ie);

        ie->id = S1AP_ProtocolIE_ID_id_RegisteredLAI;
        ie->criticality = S1AP_Criticality_ignore;
        ie->value.present =
            S1AP_InitialContextSetupRequestIEs__value_PR_LAI;

        LAI = &ie->value.choice.LAI;
        ogs_assert(LAI);

        s1ap_buffer_to_OCTET_STRING(&mme_ue->tai.plmn_id, sizeof(plmn_id_t),
                &LAI->pLMNidentity);
        ogs_assert(mme_ue->csmap);
        ogs_assert(mme_ue->p_tmsi);
        s1ap_uint16_to_OCTET_STRING(mme_ue->csmap->lai.lac, &LAI->lAC);

    } else if (mme_ue->ueRadioCapability.buf &&
            mme_ue->ueRadioCapability.size) {
        /* Set UeRadioCapability if exists */
        S1AP_UERadioCapability_t *UERadioCapability = NULL;

        ie = CALLOC(1, sizeof(S1AP_InitialContextSetupRequestIEs_t));
        ASN_SEQUENCE_ADD(&InitialContextSetupRequest->protocolIEs, ie);

        ie->id = S1AP_ProtocolIE_ID_id_UERadioCapability;
        ie->criticality = S1AP_Criticality_ignore;
        ie->value.present =
            S1AP_InitialContextSetupRequestIEs__value_PR_UERadioCapability;

        UERadioCapability = &ie->value.choice.UERadioCapability;

        ogs_assert(UERadioCapability);
        s1ap_buffer_to_OCTET_STRING(
                mme_ue->ueRadioCapability.buf, mme_ue->ueRadioCapability.size,
                UERadioCapability);
    }

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_ue_context_modification_request(
            ogs_pkbuf_t **s1apbuf, mme_ue_t *mme_ue)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_UEContextModificationRequest_t *UEContextModificationRequest = NULL;

    S1AP_UEContextModificationRequestIEs_t *ie = NULL;

    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_UESecurityCapabilities_t *UESecurityCapabilities = NULL;
    S1AP_SecurityKey_t *SecurityKey = NULL;
    S1AP_CSFallbackIndicator_t *CSFallbackIndicator = NULL;
    S1AP_LAI_t *LAI = NULL;

    enb_ue_t *enb_ue = NULL;

    ogs_assert(mme_ue);
    enb_ue = mme_ue->enb_ue;
    ogs_assert(enb_ue);

    ogs_debug("[MME] UE context modification request");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode =
        S1AP_ProcedureCode_id_UEContextModification;
    initiatingMessage->criticality = S1AP_Criticality_reject;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_UEContextModificationRequest;

    UEContextModificationRequest =
        &initiatingMessage->value.choice.UEContextModificationRequest;

    ie = CALLOC(1, sizeof(S1AP_UEContextModificationRequestIEs_t));
    ASN_SEQUENCE_ADD(&UEContextModificationRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_UEContextModificationRequestIEs__value_PR_MME_UE_S1AP_ID,

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_UEContextModificationRequestIEs_t));
    ASN_SEQUENCE_ADD(&UEContextModificationRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_UEContextModificationRequestIEs__value_PR_ENB_UE_S1AP_ID,

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    *MME_UE_S1AP_ID = enb_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = enb_ue->enb_ue_s1ap_id;

    ogs_debug("    ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            enb_ue->enb_ue_s1ap_id, enb_ue->mme_ue_s1ap_id);

    if (mme_ue->nas_eps.type == MME_EPS_TYPE_EXTENDED_SERVICE_REQUEST &&
        MME_P_TMSI_IS_AVAILABLE(mme_ue)) {
        ie = CALLOC(1, sizeof(S1AP_UEContextModificationRequestIEs_t));
        ASN_SEQUENCE_ADD(&UEContextModificationRequest->protocolIEs, ie);

        ie->id = S1AP_ProtocolIE_ID_id_CSFallbackIndicator;
        ie->criticality = S1AP_Criticality_reject;
        ie->value.present =
            S1AP_UEContextModificationRequestIEs__value_PR_CSFallbackIndicator;

        CSFallbackIndicator = &ie->value.choice.CSFallbackIndicator;
        ogs_assert(CSFallbackIndicator);

        *CSFallbackIndicator = S1AP_CSFallbackIndicator_cs_fallback_required;

        ie = CALLOC(1, sizeof(S1AP_UEContextModificationRequestIEs_t));
        ASN_SEQUENCE_ADD(&UEContextModificationRequest->protocolIEs, ie);

        ie->id = S1AP_ProtocolIE_ID_id_RegisteredLAI;
        ie->criticality = S1AP_Criticality_ignore;
        ie->value.present =
            S1AP_UEContextModificationRequestIEs__value_PR_LAI;

        LAI = &ie->value.choice.LAI;
        ogs_assert(LAI);

        s1ap_buffer_to_OCTET_STRING(&mme_ue->tai.plmn_id, sizeof(plmn_id_t),
                &LAI->pLMNidentity);
        ogs_assert(mme_ue->csmap);
        ogs_assert(mme_ue->p_tmsi);
        s1ap_uint16_to_OCTET_STRING(mme_ue->csmap->lai.lac, &LAI->lAC);

    } else {
        ie = CALLOC(1, sizeof(S1AP_UEContextModificationRequestIEs_t));
        ASN_SEQUENCE_ADD(&UEContextModificationRequest->protocolIEs, ie);

        ie->id = S1AP_ProtocolIE_ID_id_UESecurityCapabilities;
        ie->criticality = S1AP_Criticality_reject;
        ie->value.present =
            S1AP_UEContextModificationRequestIEs__value_PR_UESecurityCapabilities;

        UESecurityCapabilities = &ie->value.choice.UESecurityCapabilities;

        UESecurityCapabilities->encryptionAlgorithms.size = 2;
        UESecurityCapabilities->encryptionAlgorithms.buf = 
            CALLOC(UESecurityCapabilities->encryptionAlgorithms.size, 
                        sizeof(uint8_t));
        UESecurityCapabilities->encryptionAlgorithms.bits_unused = 0;
        UESecurityCapabilities->encryptionAlgorithms.buf[0] = 
            (mme_ue->ue_network_capability.eea << 1);

        UESecurityCapabilities->integrityProtectionAlgorithms.size = 2;
        UESecurityCapabilities->integrityProtectionAlgorithms.buf =
            CALLOC(UESecurityCapabilities->
                        integrityProtectionAlgorithms.size, sizeof(uint8_t));
        UESecurityCapabilities->integrityProtectionAlgorithms.bits_unused = 0;
        UESecurityCapabilities->integrityProtectionAlgorithms.buf[0] =
            (mme_ue->ue_network_capability.eia << 1);

        ie = CALLOC(1, sizeof(S1AP_UEContextModificationRequestIEs_t));
        ASN_SEQUENCE_ADD(&UEContextModificationRequest->protocolIEs, ie);

        ie->id = S1AP_ProtocolIE_ID_id_SecurityKey;
        ie->criticality = S1AP_Criticality_reject;
        ie->value.present =
            S1AP_UEContextModificationRequestIEs__value_PR_SecurityKey;

        SecurityKey = &ie->value.choice.SecurityKey;

        SecurityKey->size = OGS_SHA256_DIGEST_SIZE;
        SecurityKey->buf = 
            CALLOC(SecurityKey->size, sizeof(uint8_t));
        SecurityKey->bits_unused = 0;
        memcpy(SecurityKey->buf, mme_ue->kenb, SecurityKey->size);
    }

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_ue_context_release_command(
    ogs_pkbuf_t **s1apbuf, enb_ue_t *enb_ue, S1AP_Cause_PR group, long cause)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_UEContextReleaseCommand_t *UEContextReleaseCommand = NULL;

    S1AP_UEContextReleaseCommand_IEs_t *ie = NULL;
    S1AP_UE_S1AP_IDs_t *UE_S1AP_IDs = NULL;
    S1AP_Cause_t *Cause = NULL;

    ogs_assert(enb_ue);

    if (enb_ue->mme_ue_s1ap_id == 0) {
        ogs_error("invalid mme ue s1ap id");
        return OGS_ERROR;
    }

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode = S1AP_ProcedureCode_id_UEContextRelease;
    initiatingMessage->criticality = S1AP_Criticality_reject;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_UEContextReleaseCommand;

    UEContextReleaseCommand =
        &initiatingMessage->value.choice.UEContextReleaseCommand;

    ie = CALLOC(1, sizeof(S1AP_UEContextReleaseCommand_IEs_t));
    ASN_SEQUENCE_ADD(&UEContextReleaseCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_UE_S1AP_IDs;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_UEContextReleaseCommand_IEs__value_PR_UE_S1AP_IDs;

    UE_S1AP_IDs = &ie->value.choice.UE_S1AP_IDs;

    ie = CALLOC(1, sizeof(S1AP_UEContextReleaseCommand_IEs_t));
    ASN_SEQUENCE_ADD(&UEContextReleaseCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_Cause;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_UEContextReleaseCommand_IEs__value_PR_Cause;

    Cause = &ie->value.choice.Cause;

    if (enb_ue->enb_ue_s1ap_id == INVALID_UE_S1AP_ID) {
        UE_S1AP_IDs->present = S1AP_UE_S1AP_IDs_PR_mME_UE_S1AP_ID;
        UE_S1AP_IDs->choice.mME_UE_S1AP_ID = enb_ue->mme_ue_s1ap_id;
    } else {
        UE_S1AP_IDs->present = S1AP_UE_S1AP_IDs_PR_uE_S1AP_ID_pair;
        UE_S1AP_IDs->choice.uE_S1AP_ID_pair = 
            CALLOC(1, sizeof(S1AP_UE_S1AP_ID_pair_t));
        UE_S1AP_IDs->choice.uE_S1AP_ID_pair->mME_UE_S1AP_ID = 
            enb_ue->mme_ue_s1ap_id;
        UE_S1AP_IDs->choice.uE_S1AP_ID_pair->eNB_UE_S1AP_ID = 
            enb_ue->enb_ue_s1ap_id;
    }

    Cause->present = group;
    Cause->choice.radioNetwork = cause;

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}


int s1ap_build_e_rab_setup_request(
            ogs_pkbuf_t **s1apbuf, mme_bearer_t *bearer, ogs_pkbuf_t *esmbuf)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_E_RABSetupRequest_t *E_RABSetupRequest = NULL;

    S1AP_E_RABSetupRequestIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_E_RABToBeSetupListBearerSUReq_t *E_RABToBeSetupListBearerSUReq = NULL;

    S1AP_E_RABToBeSetupItemBearerSUReqIEs_t *item = NULL;
    S1AP_E_RABToBeSetupItemBearerSUReq_t *e_rab = NULL;
    S1AP_GBR_QosInformation_t *gbrQosInformation = NULL;
    S1AP_NAS_PDU_t *nasPdu = NULL;

    mme_ue_t *mme_ue = NULL;
    enb_ue_t *enb_ue = NULL;

    ogs_assert(esmbuf);
    ogs_assert(bearer);

    mme_ue = bearer->mme_ue;
    ogs_assert(mme_ue);
    enb_ue = mme_ue->enb_ue;
    ogs_assert(enb_ue);

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode = S1AP_ProcedureCode_id_E_RABSetup;
    initiatingMessage->criticality = S1AP_Criticality_reject;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_E_RABSetupRequest;

    E_RABSetupRequest = &initiatingMessage->value.choice.E_RABSetupRequest;

    ie = CALLOC(1, sizeof(S1AP_E_RABSetupRequestIEs_t));
    ASN_SEQUENCE_ADD(&E_RABSetupRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_E_RABSetupRequestIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_E_RABSetupRequestIEs_t));
    ASN_SEQUENCE_ADD(&E_RABSetupRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_E_RABSetupRequestIEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_E_RABSetupRequestIEs_t));
    ASN_SEQUENCE_ADD(&E_RABSetupRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_E_RABToBeSetupListBearerSUReq;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_E_RABSetupRequestIEs__value_PR_E_RABToBeSetupListBearerSUReq;

    E_RABToBeSetupListBearerSUReq =
        &ie->value.choice.E_RABToBeSetupListBearerSUReq;

    ogs_debug("    ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            enb_ue->enb_ue_s1ap_id, enb_ue->mme_ue_s1ap_id);

    *MME_UE_S1AP_ID = enb_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = enb_ue->enb_ue_s1ap_id;

    item = CALLOC(1, sizeof(S1AP_E_RABToBeSetupItemBearerSUReqIEs_t));
    ASN_SEQUENCE_ADD(&E_RABToBeSetupListBearerSUReq->list, item);

    item->id = S1AP_ProtocolIE_ID_id_E_RABToBeSetupItemBearerSUReq;
    item->criticality = S1AP_Criticality_reject;
    item->value.present = S1AP_E_RABToBeSetupItemBearerSUReqIEs__value_PR_E_RABToBeSetupItemBearerSUReq;

    e_rab = &item->value.choice.E_RABToBeSetupItemBearerSUReq;

    e_rab->e_RAB_ID = bearer->ebi;
    e_rab->e_RABlevelQoSParameters.qCI = bearer->qos.qci;

    ogs_debug("    EBI[%d] QCI[%d]", bearer->ebi, bearer->qos.qci);

    e_rab->e_RABlevelQoSParameters.allocationRetentionPriority.
        priorityLevel = bearer->qos.arp.priority_level;
    e_rab->e_RABlevelQoSParameters.allocationRetentionPriority.
        pre_emptionCapability = !(bearer->qos.arp.pre_emption_capability);
    e_rab->e_RABlevelQoSParameters.allocationRetentionPriority.
        pre_emptionVulnerability = !(bearer->qos.arp.pre_emption_vulnerability);

    if (bearer->qos.mbr.downlink || bearer->qos.mbr.uplink ||
        bearer->qos.gbr.downlink || bearer->qos.gbr.uplink) {
        if (bearer->qos.mbr.downlink == 0)
            bearer->qos.mbr.downlink = MAX_BIT_RATE;
        if (bearer->qos.mbr.uplink == 0)
            bearer->qos.mbr.uplink = MAX_BIT_RATE;
        if (bearer->qos.gbr.downlink == 0)
            bearer->qos.gbr.downlink = MAX_BIT_RATE;
        if (bearer->qos.gbr.uplink == 0)
            bearer->qos.gbr.uplink = MAX_BIT_RATE;

        gbrQosInformation = CALLOC(1, sizeof(S1AP_GBR_QosInformation_t));
        asn_uint642INTEGER(&gbrQosInformation->e_RAB_MaximumBitrateDL,
                bearer->qos.mbr.downlink);
        asn_uint642INTEGER(&gbrQosInformation->e_RAB_MaximumBitrateUL,
                bearer->qos.mbr.uplink);
        asn_uint642INTEGER(&gbrQosInformation->e_RAB_GuaranteedBitrateDL,
                bearer->qos.gbr.downlink);
        asn_uint642INTEGER(&gbrQosInformation->e_RAB_GuaranteedBitrateUL,
                bearer->qos.gbr.uplink);
        e_rab->e_RABlevelQoSParameters.gbrQosInformation = gbrQosInformation;
    }

    rv = s1ap_ip_to_BIT_STRING(
            &bearer->sgw_s1u_ip, &e_rab->transportLayerAddress);
    ogs_assert(rv == OGS_OK);
    s1ap_uint32_to_OCTET_STRING(bearer->sgw_s1u_teid, &e_rab->gTP_TEID);
    ogs_debug("    SGW-S1U-TEID[%d]", bearer->sgw_s1u_teid);

    nasPdu = &e_rab->nAS_PDU;
    nasPdu->size = esmbuf->len;
    nasPdu->buf = CALLOC(nasPdu->size, sizeof(uint8_t));
    memcpy(nasPdu->buf, esmbuf->data, nasPdu->size);
    ogs_pkbuf_free(esmbuf);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_e_rab_modify_request(
            ogs_pkbuf_t **s1apbuf, mme_bearer_t *bearer, ogs_pkbuf_t *esmbuf)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_E_RABModifyRequest_t *E_RABModifyRequest = NULL;

    S1AP_E_RABModifyRequestIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_E_RABToBeModifiedListBearerModReq_t
        *E_RABToBeModifiedListBearerModReq = NULL;

    S1AP_E_RABToBeModifiedItemBearerModReqIEs_t *item = NULL;
    S1AP_E_RABToBeModifiedItemBearerModReq_t *e_rab = NULL;
    S1AP_GBR_QosInformation_t *gbrQosInformation = NULL;
    S1AP_NAS_PDU_t *nasPdu = NULL;

    mme_ue_t *mme_ue = NULL;
    enb_ue_t *enb_ue = NULL;

    ogs_assert(esmbuf);
    ogs_assert(bearer);

    mme_ue = bearer->mme_ue;
    ogs_assert(mme_ue);
    enb_ue = mme_ue->enb_ue;
    ogs_assert(enb_ue);

    ogs_debug("[MME] E-RAB modify request");
    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode = S1AP_ProcedureCode_id_E_RABModify;
    initiatingMessage->criticality = S1AP_Criticality_reject;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_E_RABModifyRequest;

    E_RABModifyRequest = &initiatingMessage->value.choice.E_RABModifyRequest;

    ie = CALLOC(1, sizeof(S1AP_E_RABModifyRequestIEs_t));
    ASN_SEQUENCE_ADD(&E_RABModifyRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_E_RABModifyRequestIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_E_RABModifyRequestIEs_t));
    ASN_SEQUENCE_ADD(&E_RABModifyRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_E_RABModifyRequestIEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_E_RABModifyRequestIEs_t));
    ASN_SEQUENCE_ADD(&E_RABModifyRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_E_RABToBeModifiedListBearerModReq;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_E_RABModifyRequestIEs__value_PR_E_RABToBeModifiedListBearerModReq;

    E_RABToBeModifiedListBearerModReq =
        &ie->value.choice.E_RABToBeModifiedListBearerModReq;

    ogs_debug("    ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            enb_ue->enb_ue_s1ap_id, enb_ue->mme_ue_s1ap_id);

    *MME_UE_S1AP_ID = enb_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = enb_ue->enb_ue_s1ap_id;

    item = CALLOC(1, sizeof(S1AP_E_RABToBeModifiedItemBearerModReqIEs_t));
    ASN_SEQUENCE_ADD(&E_RABToBeModifiedListBearerModReq->list, item);

    item->id = S1AP_ProtocolIE_ID_id_E_RABToBeModifiedItemBearerModReq;
    item->criticality = S1AP_Criticality_reject;
    item->value.present = S1AP_E_RABToBeModifiedItemBearerModReqIEs__value_PR_E_RABToBeModifiedItemBearerModReq;

    e_rab = &item->value.choice.E_RABToBeModifiedItemBearerModReq;

    e_rab->e_RAB_ID = bearer->ebi;
    e_rab->e_RABLevelQoSParameters.qCI = bearer->qos.qci;

    ogs_debug("    EBI[%d] QCI[%d]", bearer->ebi, bearer->qos.qci);

    e_rab->e_RABLevelQoSParameters.allocationRetentionPriority.
        priorityLevel = bearer->qos.arp.priority_level;
    e_rab->e_RABLevelQoSParameters.allocationRetentionPriority.
        pre_emptionCapability = !(bearer->qos.arp.pre_emption_capability);
    e_rab->e_RABLevelQoSParameters.allocationRetentionPriority.
        pre_emptionVulnerability = !(bearer->qos.arp.pre_emption_vulnerability);

    if (bearer->qos.mbr.downlink || bearer->qos.mbr.uplink ||
        bearer->qos.gbr.downlink || bearer->qos.gbr.uplink) {
        if (bearer->qos.mbr.downlink == 0)
            bearer->qos.mbr.downlink = MAX_BIT_RATE;
        if (bearer->qos.mbr.uplink == 0)
            bearer->qos.mbr.uplink = MAX_BIT_RATE;
        if (bearer->qos.gbr.downlink == 0)
            bearer->qos.gbr.downlink = MAX_BIT_RATE;
        if (bearer->qos.gbr.uplink == 0)
            bearer->qos.gbr.uplink = MAX_BIT_RATE;

        gbrQosInformation = 
                CALLOC(1, sizeof(S1AP_GBR_QosInformation_t));
        asn_uint642INTEGER(&gbrQosInformation->e_RAB_MaximumBitrateDL,
                bearer->qos.mbr.downlink);
        asn_uint642INTEGER(&gbrQosInformation->e_RAB_MaximumBitrateUL,
                bearer->qos.mbr.uplink);
        asn_uint642INTEGER(&gbrQosInformation->e_RAB_GuaranteedBitrateDL,
                bearer->qos.gbr.downlink);
        asn_uint642INTEGER(&gbrQosInformation->e_RAB_GuaranteedBitrateUL,
                bearer->qos.gbr.uplink);
        e_rab->e_RABLevelQoSParameters.gbrQosInformation = gbrQosInformation;
    }

    nasPdu = &e_rab->nAS_PDU;
    nasPdu->size = esmbuf->len;
    nasPdu->buf = CALLOC(nasPdu->size, sizeof(uint8_t));
    memcpy(nasPdu->buf, esmbuf->data, nasPdu->size);
    ogs_pkbuf_free(esmbuf);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_e_rab_release_command(ogs_pkbuf_t **s1apbuf,
        mme_bearer_t *bearer, ogs_pkbuf_t *esmbuf, 
        S1AP_Cause_PR group, long cause)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_E_RABReleaseCommand_t *E_RABReleaseCommand = NULL;

    S1AP_E_RABReleaseCommandIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_UEAggregateMaximumBitrate_t *UEAggregateMaximumBitrate = NULL;
    S1AP_E_RABList_t *E_RABList = NULL;
    S1AP_NAS_PDU_t *nasPdu = NULL;

    S1AP_E_RABItemIEs_t *item = NULL;
    S1AP_E_RABItem_t *e_rab = NULL;

    mme_ue_t *mme_ue = NULL;
    enb_ue_t *enb_ue = NULL;
    s6a_subscription_data_t *subscription_data = NULL;

    ogs_assert(esmbuf);
    ogs_assert(bearer);

    mme_ue = bearer->mme_ue;
    ogs_assert(mme_ue);
    enb_ue = mme_ue->enb_ue;
    ogs_assert(enb_ue);
    subscription_data = &mme_ue->subscription_data;
    ogs_assert(subscription_data);

    ogs_debug("[MME] E-RAB release command");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode = S1AP_ProcedureCode_id_E_RABRelease;
    initiatingMessage->criticality = S1AP_Criticality_reject;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_E_RABReleaseCommand;

    E_RABReleaseCommand = &initiatingMessage->value.choice.E_RABReleaseCommand;

    ie = CALLOC(1, sizeof(S1AP_E_RABReleaseCommandIEs_t));
    ASN_SEQUENCE_ADD(&E_RABReleaseCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_E_RABReleaseCommandIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_E_RABReleaseCommandIEs_t));
    ASN_SEQUENCE_ADD(&E_RABReleaseCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_E_RABReleaseCommandIEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_E_RABReleaseCommandIEs_t));
    ASN_SEQUENCE_ADD(&E_RABReleaseCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_uEaggregateMaximumBitrate;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_E_RABReleaseCommandIEs__value_PR_UEAggregateMaximumBitrate;

    UEAggregateMaximumBitrate = &ie->value.choice.UEAggregateMaximumBitrate;

    ie = CALLOC(1, sizeof(S1AP_E_RABReleaseCommandIEs_t));
    ASN_SEQUENCE_ADD(&E_RABReleaseCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_E_RABToBeReleasedList;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_E_RABReleaseCommandIEs__value_PR_E_RABList;

    E_RABList = &ie->value.choice.E_RABList;

    ie = CALLOC(1, sizeof(S1AP_E_RABReleaseCommandIEs_t));
    ASN_SEQUENCE_ADD(&E_RABReleaseCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_NAS_PDU;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_E_RABReleaseCommandIEs__value_PR_NAS_PDU;

    nasPdu = &ie->value.choice.NAS_PDU;

    ogs_debug("    ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            enb_ue->enb_ue_s1ap_id, enb_ue->mme_ue_s1ap_id);

    *MME_UE_S1AP_ID = enb_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = enb_ue->enb_ue_s1ap_id;

    asn_uint642INTEGER(
            &UEAggregateMaximumBitrate->uEaggregateMaximumBitRateUL, 
            subscription_data->ambr.uplink);
    asn_uint642INTEGER(
            &UEAggregateMaximumBitrate->uEaggregateMaximumBitRateDL, 
            subscription_data->ambr.downlink);

    item = CALLOC(1, sizeof(S1AP_E_RABItemIEs_t));
    ASN_SEQUENCE_ADD(&E_RABList->list, item);

    item->id = S1AP_ProtocolIE_ID_id_E_RABItem;
    item->criticality = S1AP_Criticality_ignore;
    item->value.present = S1AP_E_RABItemIEs__value_PR_E_RABItem;

    e_rab = &item->value.choice.E_RABItem;

    e_rab->e_RAB_ID = bearer->ebi;
    e_rab->cause.present = group;
    e_rab->cause.choice.radioNetwork = cause;

    ogs_debug("    EBI[%d] Gruop[%d] Cause[%d]",
            bearer->ebi, group, (int)cause);

    nasPdu->size = esmbuf->len;
    nasPdu->buf = CALLOC(nasPdu->size, sizeof(uint8_t));
    memcpy(nasPdu->buf, esmbuf->data, nasPdu->size);
    ogs_pkbuf_free(esmbuf);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_paging(ogs_pkbuf_t **s1apbuf,
        mme_ue_t *mme_ue, S1AP_CNDomain_t cn_domain)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_Paging_t *Paging = NULL;

    S1AP_PagingIEs_t *ie = NULL;

    S1AP_UEIdentityIndexValue_t *UEIdentityIndexValue = NULL;
    S1AP_UEPagingID_t *UEPagingID = NULL;
    S1AP_CNDomain_t *CNDomain = NULL;
    S1AP_TAIList_t *TAIList = NULL;

    S1AP_TAIItemIEs_t *item = NULL;
    S1AP_TAIItem_t *tai_item = NULL;

    uint16_t index_value;
    uint64_t ue_imsi_value = 0;
    int i = 0;

    ogs_assert(mme_ue);

    ogs_debug("[MME] Paging");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode = S1AP_ProcedureCode_id_Paging;
    initiatingMessage->criticality = S1AP_Criticality_ignore;
    initiatingMessage->value.present = S1AP_InitiatingMessage__value_PR_Paging;

    Paging = &initiatingMessage->value.choice.Paging;

    ie = CALLOC(1, sizeof(S1AP_PagingIEs_t));
    ASN_SEQUENCE_ADD(&Paging->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_UEIdentityIndexValue;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_PagingIEs__value_PR_UEIdentityIndexValue;

    UEIdentityIndexValue = &ie->value.choice.UEIdentityIndexValue;

    ie = CALLOC(1, sizeof(S1AP_PagingIEs_t));
    ASN_SEQUENCE_ADD(&Paging->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_UEPagingID;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_PagingIEs__value_PR_UEPagingID;

    UEPagingID = &ie->value.choice.UEPagingID;

    ie = CALLOC(1, sizeof(S1AP_PagingIEs_t));
    ASN_SEQUENCE_ADD(&Paging->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_CNDomain;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_PagingIEs__value_PR_CNDomain;

    CNDomain = &ie->value.choice.CNDomain;

    ie = CALLOC(1, sizeof(S1AP_PagingIEs_t));
    ASN_SEQUENCE_ADD(&Paging->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_TAIList;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_PagingIEs__value_PR_TAIList;

    TAIList = &ie->value.choice.TAIList;

    /* Set UE Identity Index value : IMSI mod 4096 */
    UEIdentityIndexValue->size = 2;
    UEIdentityIndexValue->buf = 
        CALLOC(UEIdentityIndexValue->size, sizeof(uint8_t));

    /* Conver string to value */
    for (i = 0; i < strlen(mme_ue->imsi_bcd); i++) {
        ue_imsi_value = ue_imsi_value*10 + (mme_ue->imsi_bcd[i] - '0');
    }

    /* index(10bit) = ue_imsi_value mod 1024 */
    index_value = ue_imsi_value % 1024;
    UEIdentityIndexValue->buf[0] = index_value >> 2;
    UEIdentityIndexValue->buf[1] = (index_value & 0x3f) << 6;
    UEIdentityIndexValue->bits_unused = 6;

    /* Set Paging Identity */
    UEPagingID->present = S1AP_UEPagingID_PR_s_TMSI;
    UEPagingID->choice.s_TMSI = 
        CALLOC(1, sizeof(S1AP_S_TMSI_t));
    s1ap_uint8_to_OCTET_STRING(mme_ue->guti.mme_code, 
            &UEPagingID->choice.s_TMSI->mMEC);

    s1ap_uint32_to_OCTET_STRING(mme_ue->guti.m_tmsi, 
            &UEPagingID->choice.s_TMSI->m_TMSI);

    ogs_debug("    MME_CODE[%d] M_TMSI[0x%x]",
            mme_ue->guti.mme_code, mme_ue->guti.m_tmsi);
    ogs_debug("    CN_DOMAIN[%s]",
            cn_domain == S1AP_CNDomain_cs ? "CS" :
                cn_domain == S1AP_CNDomain_ps ? "PS" : "Unknown");

    *CNDomain = cn_domain;

    item = CALLOC(1, sizeof(S1AP_TAIItemIEs_t));
    ASN_SEQUENCE_ADD(&TAIList->list, item);

    item->id = S1AP_ProtocolIE_ID_id_TAIItem;
    item->criticality = S1AP_Criticality_ignore;
    item->value.present = S1AP_TAIItemIEs__value_PR_TAIItem;

    tai_item = &item->value.choice.TAIItem;

    s1ap_buffer_to_OCTET_STRING(&mme_ue->tai.plmn_id, sizeof(plmn_id_t),
            &tai_item->tAI.pLMNidentity);
    s1ap_uint16_to_OCTET_STRING(mme_ue->tai.tac, &tai_item->tAI.tAC);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_mme_configuration_transfer(
        ogs_pkbuf_t **s1apbuf, 
        S1AP_SONConfigurationTransfer_t *son_configuration_transfer)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_MMEConfigurationTransfer_t *MMEConfigurationTransfer = NULL;

    S1AP_MMEConfigurationTransferIEs_t *ie = NULL;
    S1AP_SONConfigurationTransfer_t *SONConfigurationTransfer = NULL;

    ogs_assert(s1apbuf);
    ogs_assert(son_configuration_transfer);

    ogs_debug("[MME] MME Configuration Transfer");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode =
        S1AP_ProcedureCode_id_MMEConfigurationTransfer;
    initiatingMessage->criticality = S1AP_Criticality_ignore;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_MMEConfigurationTransfer;

    MMEConfigurationTransfer =
        &initiatingMessage->value.choice.MMEConfigurationTransfer;

    ie = CALLOC(1, sizeof(S1AP_MMEConfigurationTransferIEs_t));
    ASN_SEQUENCE_ADD(&MMEConfigurationTransfer->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_SONConfigurationTransferMCT;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present =
        S1AP_MMEConfigurationTransferIEs__value_PR_SONConfigurationTransfer;

    SONConfigurationTransfer = &ie->value.choice.SONConfigurationTransfer;

    rv = s1ap_copy_ie(&asn_DEF_S1AP_SONConfigurationTransfer,
            son_configuration_transfer, SONConfigurationTransfer);
    ogs_assert(rv == OGS_OK);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_path_switch_ack(ogs_pkbuf_t **s1apbuf, mme_ue_t *mme_ue)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_SuccessfulOutcome_t *successfulOutcome = NULL;
    S1AP_PathSwitchRequestAcknowledge_t *PathSwitchRequestAcknowledge = NULL;

    S1AP_PathSwitchRequestAcknowledgeIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_SecurityContext_t *SecurityContext = NULL;

    enb_ue_t *enb_ue = NULL;

    ogs_assert(mme_ue);
    enb_ue = mme_ue->enb_ue;
    ogs_assert(enb_ue);

    ogs_debug("[MME] Path switch acknowledge");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_successfulOutcome;
    pdu.choice.successfulOutcome = 
        CALLOC(1, sizeof(S1AP_SuccessfulOutcome_t));

    successfulOutcome = pdu.choice.successfulOutcome;
    successfulOutcome->procedureCode = S1AP_ProcedureCode_id_PathSwitchRequest;
    successfulOutcome->criticality = S1AP_Criticality_reject;
    successfulOutcome->value.present =
        S1AP_SuccessfulOutcome__value_PR_PathSwitchRequestAcknowledge;

    PathSwitchRequestAcknowledge =
        &successfulOutcome->value.choice.PathSwitchRequestAcknowledge;

    ie = CALLOC(1, sizeof(S1AP_PathSwitchRequestAcknowledgeIEs_t));
    ASN_SEQUENCE_ADD(&PathSwitchRequestAcknowledge->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present =
        S1AP_PathSwitchRequestAcknowledgeIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_PathSwitchRequestAcknowledgeIEs_t));
    ASN_SEQUENCE_ADD(&PathSwitchRequestAcknowledge->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present =
        S1AP_PathSwitchRequestAcknowledgeIEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_PathSwitchRequestAcknowledgeIEs_t));
    ASN_SEQUENCE_ADD(&PathSwitchRequestAcknowledge->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_SecurityContext;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_PathSwitchRequestAcknowledgeIEs__value_PR_SecurityContext;

    SecurityContext = &ie->value.choice.SecurityContext;

    ogs_debug("    ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            enb_ue->enb_ue_s1ap_id, enb_ue->mme_ue_s1ap_id);

    *MME_UE_S1AP_ID = enb_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = enb_ue->enb_ue_s1ap_id;

    SecurityContext->nextHopChainingCount = mme_ue->nhcc;
    SecurityContext->nextHopParameter.size = OGS_SHA256_DIGEST_SIZE;
    SecurityContext->nextHopParameter.buf = 
        CALLOC(SecurityContext->nextHopParameter.size,
        sizeof(uint8_t));
    SecurityContext->nextHopParameter.bits_unused = 0;
    memcpy(SecurityContext->nextHopParameter.buf,
            mme_ue->nh, SecurityContext->nextHopParameter.size);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_path_switch_failure(ogs_pkbuf_t **s1apbuf,
    uint32_t enb_ue_s1ap_id, uint32_t mme_ue_s1ap_id,
    S1AP_Cause_PR group, long cause)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_UnsuccessfulOutcome_t *unsuccessfulOutcome = NULL;
    S1AP_PathSwitchRequestFailure_t *PathSwitchRequestFailure = NULL;

    S1AP_PathSwitchRequestFailureIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_Cause_t *Cause = NULL;

    ogs_debug("[MME] Path switch failure");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_unsuccessfulOutcome;
    pdu.choice.unsuccessfulOutcome = 
        CALLOC(1, sizeof(S1AP_UnsuccessfulOutcome_t));

    unsuccessfulOutcome = pdu.choice.unsuccessfulOutcome;
    unsuccessfulOutcome->procedureCode =
        S1AP_ProcedureCode_id_PathSwitchRequest;
    unsuccessfulOutcome->criticality = S1AP_Criticality_reject;
    unsuccessfulOutcome->value.present =
        S1AP_UnsuccessfulOutcome__value_PR_PathSwitchRequestFailure;

    PathSwitchRequestFailure =
        &unsuccessfulOutcome->value.choice.PathSwitchRequestFailure;

    ie = CALLOC(1, sizeof(S1AP_PathSwitchRequestFailureIEs_t));
    ASN_SEQUENCE_ADD(&PathSwitchRequestFailure->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present =
        S1AP_PathSwitchRequestFailureIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_PathSwitchRequestFailureIEs_t));
    ASN_SEQUENCE_ADD(&PathSwitchRequestFailure->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present =
        S1AP_PathSwitchRequestFailureIEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_PathSwitchRequestFailureIEs_t));
    ASN_SEQUENCE_ADD(&PathSwitchRequestFailure->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_Cause;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_PathSwitchRequestFailureIEs__value_PR_Cause;

    Cause = &ie->value.choice.Cause;

    ogs_debug("    ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            enb_ue_s1ap_id, mme_ue_s1ap_id);
    ogs_debug("    Group[%d] Cause[%d]", group, (int)cause);

    *MME_UE_S1AP_ID = mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = enb_ue_s1ap_id;
    Cause->present = group;
    Cause->choice.radioNetwork = cause;

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_handover_command(ogs_pkbuf_t **s1apbuf, enb_ue_t *source_ue)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_SuccessfulOutcome_t *successfulOutcome = NULL;
    S1AP_HandoverCommand_t *HandoverCommand = NULL;

    S1AP_HandoverCommandIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_HandoverType_t *HandoverType = NULL;
    S1AP_E_RABSubjecttoDataForwardingList_t
        *E_RABSubjecttoDataForwardingList = NULL;
    S1AP_Target_ToSource_TransparentContainer_t *Target_ToSource_TransparentContainer = NULL;

    mme_ue_t *mme_ue = NULL;
    mme_sess_t *sess = NULL;
    mme_bearer_t *bearer = NULL;

    ogs_assert(source_ue);
    mme_ue = source_ue->mme_ue;

    ogs_debug("[MME] Handover command");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_successfulOutcome;
    pdu.choice.successfulOutcome = 
        CALLOC(1, sizeof(S1AP_SuccessfulOutcome_t));

    successfulOutcome = pdu.choice.successfulOutcome;
    successfulOutcome->procedureCode =
        S1AP_ProcedureCode_id_HandoverPreparation;
    successfulOutcome->criticality = S1AP_Criticality_reject;
    successfulOutcome->value.present =
        S1AP_SuccessfulOutcome__value_PR_HandoverCommand;

    HandoverCommand = &successfulOutcome->value.choice.HandoverCommand;
    ogs_assert(HandoverCommand);

    ie = CALLOC(1, sizeof(S1AP_HandoverCommandIEs_t));
    ASN_SEQUENCE_ADD(&HandoverCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_HandoverCommandIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_HandoverCommandIEs_t));
    ASN_SEQUENCE_ADD(&HandoverCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_HandoverCommandIEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_HandoverCommandIEs_t));
    ASN_SEQUENCE_ADD(&HandoverCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_HandoverType;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_HandoverCommandIEs__value_PR_HandoverType;

    HandoverType = &ie->value.choice.HandoverType;

    *MME_UE_S1AP_ID = source_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = source_ue->enb_ue_s1ap_id;
    *HandoverType = source_ue->handover_type;

    ogs_debug("    ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            source_ue->enb_ue_s1ap_id, source_ue->mme_ue_s1ap_id);

    sess = mme_sess_first(mme_ue);
    while (sess) {
        bearer = mme_bearer_first(sess);
        while (bearer) {
            S1AP_E_RABDataForwardingItem_t *e_rab = NULL;

            if (MME_HAVE_SGW_DL_INDIRECT_TUNNEL(bearer) ||
                MME_HAVE_SGW_UL_INDIRECT_TUNNEL(bearer)) {
                S1AP_E_RABDataForwardingItemIEs_t *item = NULL;

                if (E_RABSubjecttoDataForwardingList == NULL) {
                    ie = CALLOC(1, sizeof(S1AP_HandoverCommandIEs_t));
                    ogs_assert(ie);
                    ASN_SEQUENCE_ADD(&HandoverCommand->protocolIEs, ie);

                    ie->id = S1AP_ProtocolIE_ID_id_E_RABSubjecttoDataForwardingList;
                    ie->criticality = S1AP_Criticality_ignore;
                    ie->value.present =
                        S1AP_HandoverCommandIEs__value_PR_E_RABSubjecttoDataForwardingList;

                    E_RABSubjecttoDataForwardingList =
                        &ie->value.choice.E_RABSubjecttoDataForwardingList;
                }
                ogs_assert(E_RABSubjecttoDataForwardingList);

                item = CALLOC(
                        1, sizeof(S1AP_E_RABDataForwardingItemIEs_t));
                ogs_assert(item);
                ASN_SEQUENCE_ADD(&E_RABSubjecttoDataForwardingList->list, item);

                item->id = S1AP_ProtocolIE_ID_id_E_RABDataForwardingItem;
                item->criticality = S1AP_Criticality_ignore;
                item->value.present =
                    S1AP_E_RABDataForwardingItemIEs__value_PR_E_RABDataForwardingItem;

                e_rab = &item->value.choice.E_RABDataForwardingItem;
                ogs_assert(e_rab);

                e_rab->e_RAB_ID = bearer->ebi;
            }

            if (MME_HAVE_SGW_DL_INDIRECT_TUNNEL(bearer)) {
                ogs_assert(e_rab);
                e_rab->dL_transportLayerAddress =
                    (S1AP_TransportLayerAddress_t *)
                    CALLOC(1, sizeof(S1AP_TransportLayerAddress_t));
                rv = s1ap_ip_to_BIT_STRING(
                        &bearer->sgw_dl_ip, e_rab->dL_transportLayerAddress);
                ogs_assert(rv == OGS_OK);

                e_rab->dL_gTP_TEID = (S1AP_GTP_TEID_t *)
                    CALLOC(1, sizeof(S1AP_GTP_TEID_t));
                s1ap_uint32_to_OCTET_STRING(
                        bearer->sgw_dl_teid, e_rab->dL_gTP_TEID);
                ogs_debug("    SGW-DL-TEID[%d]", bearer->sgw_dl_teid);
            }

            if (MME_HAVE_SGW_UL_INDIRECT_TUNNEL(bearer)) {
                ogs_assert(e_rab);
                e_rab->uL_TransportLayerAddress =
                    (S1AP_TransportLayerAddress_t *)
                    CALLOC(1, sizeof(S1AP_TransportLayerAddress_t));
                rv = s1ap_ip_to_BIT_STRING(
                    &bearer->sgw_ul_ip, e_rab->uL_TransportLayerAddress);
                ogs_assert(rv == OGS_OK);

                e_rab->uL_GTP_TEID = (S1AP_GTP_TEID_t *)
                    CALLOC(1, sizeof(S1AP_GTP_TEID_t));
                s1ap_uint32_to_OCTET_STRING(
                        bearer->sgw_ul_teid, e_rab->uL_GTP_TEID);
                ogs_debug("    SGW-UL-TEID[%d]", bearer->sgw_dl_teid);
            }

            bearer = mme_bearer_next(bearer);
        }
        sess = mme_sess_next(sess);
    }

    ie = CALLOC(1, sizeof(S1AP_HandoverCommandIEs_t));
    ASN_SEQUENCE_ADD(&HandoverCommand->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_Target_ToSource_TransparentContainer;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_HandoverCommandIEs__value_PR_Target_ToSource_TransparentContainer;

    Target_ToSource_TransparentContainer =
        &ie->value.choice.Target_ToSource_TransparentContainer;

    s1ap_buffer_to_OCTET_STRING(mme_ue->container.buf, mme_ue->container.size, 
            Target_ToSource_TransparentContainer);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_handover_preparation_failure(
        ogs_pkbuf_t **s1apbuf, enb_ue_t *source_ue, S1AP_Cause_t *cause)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_UnsuccessfulOutcome_t *unsuccessfulOutcome = NULL;
    S1AP_HandoverPreparationFailure_t *HandoverPreparationFailure = NULL;

    S1AP_HandoverPreparationFailureIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_Cause_t *Cause = NULL;

    ogs_assert(s1apbuf);
    ogs_assert(source_ue);
    ogs_assert(cause);

    ogs_debug("[MME] Handover preparation failure");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_unsuccessfulOutcome;
    pdu.choice.unsuccessfulOutcome = 
        CALLOC(1, sizeof(S1AP_UnsuccessfulOutcome_t));

    unsuccessfulOutcome = pdu.choice.unsuccessfulOutcome;
    unsuccessfulOutcome->procedureCode =
        S1AP_ProcedureCode_id_HandoverPreparation;
    unsuccessfulOutcome->criticality = S1AP_Criticality_reject;
    unsuccessfulOutcome->value.present =
        S1AP_UnsuccessfulOutcome__value_PR_HandoverPreparationFailure;

    HandoverPreparationFailure =
        &unsuccessfulOutcome->value.choice.HandoverPreparationFailure;

    ie = CALLOC(1, sizeof(S1AP_HandoverPreparationFailureIEs_t));
    ASN_SEQUENCE_ADD(&HandoverPreparationFailure->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present =
        S1AP_HandoverPreparationFailureIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_HandoverPreparationFailureIEs_t));
    ASN_SEQUENCE_ADD(&HandoverPreparationFailure->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present =
        S1AP_HandoverPreparationFailureIEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_HandoverPreparationFailureIEs_t));
    ASN_SEQUENCE_ADD(&HandoverPreparationFailure->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_Cause;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_HandoverPreparationFailureIEs__value_PR_Cause;

    Cause = &ie->value.choice.Cause;

    ogs_debug("    ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            source_ue->enb_ue_s1ap_id, source_ue->mme_ue_s1ap_id);
    ogs_debug("    Group[%d] Cause[%d]",
            cause->present, (int)cause->choice.radioNetwork);

    *MME_UE_S1AP_ID = source_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = source_ue->enb_ue_s1ap_id;
    Cause->present = cause->present;
    Cause->choice.radioNetwork = cause->choice.radioNetwork;

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_handover_request(
        ogs_pkbuf_t **s1apbuf, mme_ue_t *mme_ue, enb_ue_t *target_ue,
        S1AP_ENB_UE_S1AP_ID_t *enb_ue_s1ap_id,
        S1AP_MME_UE_S1AP_ID_t *mme_ue_s1ap_id,
        S1AP_HandoverType_t *handovertype,
        S1AP_Cause_t *cause,
        S1AP_Source_ToTarget_TransparentContainer_t
            *source_totarget_transparentContainer)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_HandoverRequest_t *HandoverRequest = NULL;

    S1AP_HandoverRequestIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_HandoverType_t *HandoverType = NULL;
    S1AP_Cause_t *Cause = NULL;
    S1AP_UEAggregateMaximumBitrate_t *UEAggregateMaximumBitrate = NULL;
    S1AP_E_RABToBeSetupListHOReq_t *E_RABToBeSetupListHOReq = NULL;
    S1AP_Source_ToTarget_TransparentContainer_t
        *Source_ToTarget_TransparentContainer = NULL;
    S1AP_UESecurityCapabilities_t *UESecurityCapabilities = NULL;
    S1AP_SecurityContext_t *SecurityContext = NULL;

    mme_sess_t *sess = NULL;
    mme_bearer_t *bearer = NULL;
    s6a_subscription_data_t *subscription_data = NULL;

    ogs_assert(handovertype);
    ogs_assert(cause);
    ogs_assert(source_totarget_transparentContainer);

    ogs_assert(target_ue);
    ogs_assert(mme_ue);
    subscription_data = &mme_ue->subscription_data;
    ogs_assert(subscription_data);

    ogs_debug("[MME] Handover request");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode =
        S1AP_ProcedureCode_id_HandoverResourceAllocation;
    initiatingMessage->criticality = S1AP_Criticality_reject;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_HandoverRequest;

    HandoverRequest = &initiatingMessage->value.choice.HandoverRequest;

    ie = CALLOC(1, sizeof(S1AP_HandoverRequestIEs_t));
    ASN_SEQUENCE_ADD(&HandoverRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_HandoverRequestIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_HandoverRequestIEs_t));
    ASN_SEQUENCE_ADD(&HandoverRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_HandoverType;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_HandoverRequestIEs__value_PR_HandoverType;

    HandoverType = &ie->value.choice.HandoverType;

    ie = CALLOC(1, sizeof(S1AP_HandoverRequestIEs_t));
    ASN_SEQUENCE_ADD(&HandoverRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_Cause;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_HandoverRequestIEs__value_PR_Cause;

    Cause = &ie->value.choice.Cause;

    ie = CALLOC(1, sizeof(S1AP_HandoverRequestIEs_t));
    ASN_SEQUENCE_ADD(&HandoverRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_uEaggregateMaximumBitrate;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_HandoverRequestIEs__value_PR_UEAggregateMaximumBitrate;

    UEAggregateMaximumBitrate = &ie->value.choice.UEAggregateMaximumBitrate;

    ie = CALLOC(1, sizeof(S1AP_HandoverRequestIEs_t));
    ASN_SEQUENCE_ADD(&HandoverRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_E_RABToBeSetupListHOReq;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_HandoverRequestIEs__value_PR_E_RABToBeSetupListHOReq;

    E_RABToBeSetupListHOReq = &ie->value.choice.E_RABToBeSetupListHOReq;

    ie = CALLOC(1, sizeof(S1AP_HandoverRequestIEs_t));
    ASN_SEQUENCE_ADD(&HandoverRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_Source_ToTarget_TransparentContainer;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_HandoverRequestIEs__value_PR_Source_ToTarget_TransparentContainer;

    Source_ToTarget_TransparentContainer =
        &ie->value.choice.Source_ToTarget_TransparentContainer;

    ie = CALLOC(1, sizeof(S1AP_HandoverRequestIEs_t));
    ASN_SEQUENCE_ADD(&HandoverRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_UESecurityCapabilities;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_HandoverRequestIEs__value_PR_UESecurityCapabilities;

    UESecurityCapabilities = &ie->value.choice.UESecurityCapabilities;

    ie = CALLOC(1, sizeof(S1AP_HandoverRequestIEs_t));
    ASN_SEQUENCE_ADD(&HandoverRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_SecurityContext;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
        S1AP_HandoverRequestIEs__value_PR_SecurityContext;

    SecurityContext = &ie->value.choice.SecurityContext;

    *MME_UE_S1AP_ID = target_ue->mme_ue_s1ap_id;
    *HandoverType = *handovertype;
    Cause->present = cause->present;
    Cause->choice.radioNetwork = cause->choice.radioNetwork;

    asn_uint642INTEGER(
            &UEAggregateMaximumBitrate->uEaggregateMaximumBitRateUL, 
            subscription_data->ambr.uplink);
    asn_uint642INTEGER(
            &UEAggregateMaximumBitrate->uEaggregateMaximumBitRateDL, 
            subscription_data->ambr.downlink);

    sess = mme_sess_first(mme_ue);
    while (sess) {
        bearer = mme_bearer_first(sess);
        while (bearer) {
            S1AP_E_RABToBeSetupItemHOReqIEs_t *item = NULL;
            S1AP_E_RABToBeSetupItemHOReq_t *e_rab = NULL;
            S1AP_GBR_QosInformation_t *gbrQosInformation = NULL;

            item = CALLOC(1, sizeof(S1AP_E_RABToBeSetupItemHOReqIEs_t));
            ASN_SEQUENCE_ADD(&E_RABToBeSetupListHOReq->list, item);

            item->id = S1AP_ProtocolIE_ID_id_E_RABToBeSetupItemHOReq;
            item->criticality = S1AP_Criticality_reject;
            item->value.present =
            S1AP_E_RABToBeSetupItemHOReqIEs__value_PR_E_RABToBeSetupItemHOReq;

            e_rab = &item->value.choice.E_RABToBeSetupItemHOReq;

            e_rab->e_RAB_ID = bearer->ebi;
            e_rab->e_RABlevelQosParameters.qCI = bearer->qos.qci;

            e_rab->e_RABlevelQosParameters.allocationRetentionPriority.
                priorityLevel = bearer->qos.arp.priority_level;
            e_rab->e_RABlevelQosParameters.allocationRetentionPriority.
                pre_emptionCapability =
                    !(bearer->qos.arp.pre_emption_capability);
            e_rab->e_RABlevelQosParameters.allocationRetentionPriority.
                pre_emptionVulnerability =
                    !(bearer->qos.arp.pre_emption_vulnerability);

            if (bearer->qos.mbr.downlink || bearer->qos.mbr.uplink ||
                bearer->qos.gbr.downlink || bearer->qos.gbr.uplink) {
                if (bearer->qos.mbr.downlink == 0)
                    bearer->qos.mbr.downlink = MAX_BIT_RATE;
                if (bearer->qos.mbr.uplink == 0)
                    bearer->qos.mbr.uplink = MAX_BIT_RATE;
                if (bearer->qos.gbr.downlink == 0)
                    bearer->qos.gbr.downlink = MAX_BIT_RATE;
                if (bearer->qos.gbr.uplink == 0)
                    bearer->qos.gbr.uplink = MAX_BIT_RATE;

                gbrQosInformation = 
                        CALLOC(1, sizeof(struct S1AP_GBR_QosInformation));
                asn_uint642INTEGER(&gbrQosInformation->e_RAB_MaximumBitrateDL,
                        bearer->qos.mbr.downlink);
                asn_uint642INTEGER(&gbrQosInformation->e_RAB_MaximumBitrateUL,
                        bearer->qos.mbr.uplink);
                asn_uint642INTEGER(&gbrQosInformation->
                        e_RAB_GuaranteedBitrateDL, bearer->qos.gbr.downlink);
                asn_uint642INTEGER(&gbrQosInformation->
                        e_RAB_GuaranteedBitrateUL, bearer->qos.gbr.uplink);
                e_rab->e_RABlevelQosParameters.gbrQosInformation =
                        gbrQosInformation;
            }

            rv = s1ap_ip_to_BIT_STRING(
                    &bearer->sgw_s1u_ip, &e_rab->transportLayerAddress);
            ogs_assert(rv == OGS_OK);
            s1ap_uint32_to_OCTET_STRING(bearer->sgw_s1u_teid, &e_rab->gTP_TEID);
            ogs_debug("    SGW-S1U-TEID[%d]", bearer->sgw_s1u_teid);

            bearer = mme_bearer_next(bearer);
        }
        sess = mme_sess_next(sess);
    }

    s1ap_buffer_to_OCTET_STRING(
            source_totarget_transparentContainer->buf, 
            source_totarget_transparentContainer->size, 
            Source_ToTarget_TransparentContainer);

    UESecurityCapabilities->encryptionAlgorithms.size = 2;
    UESecurityCapabilities->encryptionAlgorithms.buf = 
        CALLOC(UESecurityCapabilities->encryptionAlgorithms.size, 
                    sizeof(uint8_t));
    UESecurityCapabilities->encryptionAlgorithms.bits_unused = 0;
    UESecurityCapabilities->encryptionAlgorithms.buf[0] = 
        (mme_ue->ue_network_capability.eea << 1);

    UESecurityCapabilities->integrityProtectionAlgorithms.size = 2;
    UESecurityCapabilities->integrityProtectionAlgorithms.buf =
        CALLOC(UESecurityCapabilities->
                        integrityProtectionAlgorithms.size, sizeof(uint8_t));
    UESecurityCapabilities->integrityProtectionAlgorithms.bits_unused = 0;
    UESecurityCapabilities->integrityProtectionAlgorithms.buf[0] =
        (mme_ue->ue_network_capability.eia << 1);

    SecurityContext->nextHopChainingCount = mme_ue->nhcc;
    SecurityContext->nextHopParameter.size = OGS_SHA256_DIGEST_SIZE;
    SecurityContext->nextHopParameter.buf = 
        CALLOC(SecurityContext->nextHopParameter.size,
        sizeof(uint8_t));
    SecurityContext->nextHopParameter.bits_unused = 0;
    memcpy(SecurityContext->nextHopParameter.buf,
            mme_ue->nh, SecurityContext->nextHopParameter.size);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_handover_cancel_ack(ogs_pkbuf_t **s1apbuf, enb_ue_t *source_ue)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_SuccessfulOutcome_t *successfulOutcome = NULL;
    S1AP_HandoverCancelAcknowledge_t *HandoverCancelAcknowledge = NULL;

    S1AP_HandoverCancelAcknowledgeIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;

    ogs_assert(source_ue);

    ogs_debug("[MME] Handover cancel acknowledge");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_successfulOutcome;
    pdu.choice.successfulOutcome = 
        CALLOC(1, sizeof(S1AP_SuccessfulOutcome_t));

    successfulOutcome = pdu.choice.successfulOutcome;
    successfulOutcome->procedureCode = S1AP_ProcedureCode_id_HandoverCancel;
    successfulOutcome->criticality = S1AP_Criticality_reject;
    successfulOutcome->value.present =
        S1AP_SuccessfulOutcome__value_PR_HandoverCancelAcknowledge;

    HandoverCancelAcknowledge =
        &successfulOutcome->value.choice.HandoverCancelAcknowledge;

    ie = CALLOC(1, sizeof(S1AP_HandoverCancelAcknowledgeIEs_t));
    ASN_SEQUENCE_ADD(&HandoverCancelAcknowledge->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present =
        S1AP_HandoverCancelAcknowledgeIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_HandoverCancelAcknowledgeIEs_t));
    ASN_SEQUENCE_ADD(&HandoverCancelAcknowledge->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present =
        S1AP_HandoverCancelAcknowledgeIEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    *MME_UE_S1AP_ID = source_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = source_ue->enb_ue_s1ap_id;

    ogs_debug("    Source : ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            source_ue->enb_ue_s1ap_id, source_ue->mme_ue_s1ap_id);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_mme_status_transfer(ogs_pkbuf_t **s1apbuf,
        enb_ue_t *target_ue,
        S1AP_ENB_StatusTransfer_TransparentContainer_t
            *enb_statustransfer_transparentContainer)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_MMEStatusTransfer_t *MMEStatusTransfer = NULL;

    S1AP_MMEStatusTransferIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_ENB_StatusTransfer_TransparentContainer_t
        *ENB_StatusTransfer_TransparentContainer = NULL;

    ogs_assert(target_ue);
    ogs_assert(enb_statustransfer_transparentContainer);
    
    ogs_debug("[MME] MME status transfer");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode = S1AP_ProcedureCode_id_MMEStatusTransfer;
    initiatingMessage->criticality = S1AP_Criticality_ignore;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_MMEStatusTransfer;

    MMEStatusTransfer = &initiatingMessage->value.choice.MMEStatusTransfer;

    ie = CALLOC(1, sizeof(S1AP_MMEStatusTransferIEs_t));
    ASN_SEQUENCE_ADD(&MMEStatusTransfer->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_MMEStatusTransferIEs__value_PR_MME_UE_S1AP_ID;

    MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_MMEStatusTransferIEs_t));
    ASN_SEQUENCE_ADD(&MMEStatusTransfer->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_MMEStatusTransferIEs__value_PR_ENB_UE_S1AP_ID;

    ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

    ie = CALLOC(1, sizeof(S1AP_MMEStatusTransferIEs_t));
    ASN_SEQUENCE_ADD(&MMEStatusTransfer->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_eNB_StatusTransfer_TransparentContainer;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present =
    S1AP_MMEStatusTransferIEs__value_PR_ENB_StatusTransfer_TransparentContainer;

    ENB_StatusTransfer_TransparentContainer =
        &ie->value.choice.ENB_StatusTransfer_TransparentContainer;

    *MME_UE_S1AP_ID = target_ue->mme_ue_s1ap_id;
    *ENB_UE_S1AP_ID = target_ue->enb_ue_s1ap_id;

    ogs_debug("    Target : ENB_UE_S1AP_ID[%d] MME_UE_S1AP_ID[%d]",
            target_ue->enb_ue_s1ap_id, target_ue->mme_ue_s1ap_id);

    rv = s1ap_copy_ie(
            &asn_DEF_S1AP_ENB_StatusTransfer_TransparentContainer,
            enb_statustransfer_transparentContainer,
            ENB_StatusTransfer_TransparentContainer);
    ogs_assert(rv == OGS_OK);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_error_indication(
        ogs_pkbuf_t **s1apbuf,
        S1AP_MME_UE_S1AP_ID_t *mme_ue_s1ap_id,
        S1AP_ENB_UE_S1AP_ID_t *enb_ue_s1ap_id,
        S1AP_Cause_PR group, long cause)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_ErrorIndication_t *ErrorIndication = NULL;

    S1AP_ErrorIndicationIEs_t *ie = NULL;
    S1AP_MME_UE_S1AP_ID_t *MME_UE_S1AP_ID = NULL;
    S1AP_ENB_UE_S1AP_ID_t *ENB_UE_S1AP_ID = NULL;
    S1AP_Cause_t *Cause = NULL;

    ogs_debug("[MME] Error Indication");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode = S1AP_ProcedureCode_id_ErrorIndication;
    initiatingMessage->criticality = S1AP_Criticality_ignore;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_ErrorIndication;

    ErrorIndication = &initiatingMessage->value.choice.ErrorIndication;

    if (mme_ue_s1ap_id) {
        ie = CALLOC(1, sizeof(S1AP_ErrorIndicationIEs_t));
        ASN_SEQUENCE_ADD(&ErrorIndication->protocolIEs, ie);

        ie->id = S1AP_ProtocolIE_ID_id_MME_UE_S1AP_ID;
        ie->criticality = S1AP_Criticality_ignore;
        ie->value.present = S1AP_ErrorIndicationIEs__value_PR_MME_UE_S1AP_ID;

        MME_UE_S1AP_ID = &ie->value.choice.MME_UE_S1AP_ID;

        *MME_UE_S1AP_ID = *mme_ue_s1ap_id;
        ogs_debug("    MME_UE_S1AP_ID[%d]", (int)*mme_ue_s1ap_id);
    }

    if (enb_ue_s1ap_id) {
        ie = CALLOC(1, sizeof(S1AP_ErrorIndicationIEs_t));
        ASN_SEQUENCE_ADD(&ErrorIndication->protocolIEs, ie);

        ie->id = S1AP_ProtocolIE_ID_id_eNB_UE_S1AP_ID;
        ie->criticality = S1AP_Criticality_ignore;
        ie->value.present = S1AP_ErrorIndicationIEs__value_PR_ENB_UE_S1AP_ID;

        ENB_UE_S1AP_ID = &ie->value.choice.ENB_UE_S1AP_ID;

        *ENB_UE_S1AP_ID = *enb_ue_s1ap_id;
        ogs_debug("    ENB_UE_S1AP_ID[%d]", (int)*enb_ue_s1ap_id);
    }

    ie = CALLOC(1, sizeof(S1AP_ErrorIndicationIEs_t));
    ASN_SEQUENCE_ADD(&ErrorIndication->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_Cause;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_ErrorIndicationIEs__value_PR_Cause;

    Cause = &ie->value.choice.Cause;

    Cause->present = group;
    Cause->choice.radioNetwork = cause;

    ogs_debug("    Group[%d] Cause[%d]",
            Cause->present, (int)Cause->choice.radioNetwork);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_s1_reset(
        ogs_pkbuf_t **s1apbuf,
        S1AP_Cause_PR group, long cause,
        S1AP_UE_associatedLogicalS1_ConnectionListRes_t *partOfS1_Interface)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_Reset_t *Reset = NULL;

    S1AP_ResetIEs_t *ie = NULL;
    S1AP_Cause_t *Cause = NULL;
    S1AP_ResetType_t *ResetType = NULL;

    ogs_debug("[MME] Reset");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode = S1AP_ProcedureCode_id_Reset;
    initiatingMessage->criticality = S1AP_Criticality_ignore;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_Reset;

    Reset = &initiatingMessage->value.choice.Reset;

    ie = CALLOC(1, sizeof(S1AP_ResetIEs_t));
    ASN_SEQUENCE_ADD(&Reset->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_Cause;
    ie->criticality = S1AP_Criticality_ignore;
    ie->value.present = S1AP_ResetIEs__value_PR_Cause;

    Cause = &ie->value.choice.Cause;

    ie = CALLOC(1, sizeof(S1AP_ResetIEs_t));
    ASN_SEQUENCE_ADD(&Reset->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_ResetType;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_ResetIEs__value_PR_ResetType;

    ResetType = &ie->value.choice.ResetType;

    Cause->present = group;
    Cause->choice.radioNetwork = cause;

    ogs_debug("    Group[%d] Cause[%d] partOfS1_Interface[%p]",
        Cause->present, (int)Cause->choice.radioNetwork, partOfS1_Interface);

    if (partOfS1_Interface) {
        ResetType->present = S1AP_ResetType_PR_partOfS1_Interface;
        ResetType->choice.partOfS1_Interface = partOfS1_Interface;
    } else {
        ResetType->present = S1AP_ResetType_PR_s1_Interface;
        ResetType->choice.s1_Interface = S1AP_ResetAll_reset_all;
    }

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_s1_reset_partial(
        ogs_pkbuf_t **s1apbuf,
        S1AP_Cause_PR group, long cause,
        S1AP_MME_UE_S1AP_ID_t *mme_ue_s1ap_id,
        S1AP_ENB_UE_S1AP_ID_t *enb_ue_s1ap_id)
{
    S1AP_UE_associatedLogicalS1_ConnectionListRes_t *partOfS1_Interface = NULL;
    S1AP_UE_associatedLogicalS1_ConnectionItemRes_t *ie2 = NULL;
    S1AP_UE_associatedLogicalS1_ConnectionItem_t *item = NULL;

    partOfS1_Interface = CALLOC(1,
            sizeof(S1AP_UE_associatedLogicalS1_ConnectionListRes_t));
    ogs_assert(partOfS1_Interface);

    ie2 = CALLOC(1,
            sizeof(S1AP_UE_associatedLogicalS1_ConnectionItemRes_t));
    ASN_SEQUENCE_ADD(&partOfS1_Interface->list, ie2);

    ie2->id = S1AP_ProtocolIE_ID_id_UE_associatedLogicalS1_ConnectionItem;
    ie2->criticality = S1AP_Criticality_reject;
    ie2->value.present = S1AP_UE_associatedLogicalS1_ConnectionItemRes__value_PR_UE_associatedLogicalS1_ConnectionItem;

    item = &ie2->value.choice.UE_associatedLogicalS1_ConnectionItem;
    item->mME_UE_S1AP_ID = mme_ue_s1ap_id;
    item->eNB_UE_S1AP_ID = enb_ue_s1ap_id;

    return s1ap_build_s1_reset(s1apbuf, group, cause, partOfS1_Interface);
}

int s1ap_build_s1_reset_ack(
        ogs_pkbuf_t **s1apbuf,
        S1AP_UE_associatedLogicalS1_ConnectionListRes_t *partOfS1_Interface)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_SuccessfulOutcome_t *successfulOutcome = NULL;
    S1AP_ResetAcknowledge_t *ResetAcknowledge = NULL;

    S1AP_ResetAcknowledgeIEs_t *ie = NULL;

    ogs_debug("[MME] Reset acknowledge");

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_successfulOutcome;
    pdu.choice.successfulOutcome = 
        CALLOC(1, sizeof(S1AP_SuccessfulOutcome_t));

    successfulOutcome = pdu.choice.successfulOutcome;
    successfulOutcome->procedureCode = S1AP_ProcedureCode_id_Reset;
    successfulOutcome->criticality = S1AP_Criticality_reject;
    successfulOutcome->value.present =
        S1AP_SuccessfulOutcome__value_PR_ResetAcknowledge;

    ResetAcknowledge = &successfulOutcome->value.choice.ResetAcknowledge;

    if (partOfS1_Interface && partOfS1_Interface->list.count) {
        int i = 0;
        S1AP_UE_associatedLogicalS1_ConnectionListResAck_t *list = NULL;

        ie = CALLOC(1, sizeof(S1AP_ResetAcknowledgeIEs_t));
        ASN_SEQUENCE_ADD(&ResetAcknowledge->protocolIEs, ie);

        ie->id =
            S1AP_ProtocolIE_ID_id_UE_associatedLogicalS1_ConnectionListResAck;
        ie->criticality = S1AP_Criticality_ignore;
        ie->value.present = S1AP_ResetAcknowledgeIEs__value_PR_UE_associatedLogicalS1_ConnectionListResAck;

        list = &ie->value.choice.UE_associatedLogicalS1_ConnectionListResAck;

        for (i = 0; i < partOfS1_Interface->list.count; i++) {
            S1AP_UE_associatedLogicalS1_ConnectionItemRes_t *ie1 = NULL;
            S1AP_UE_associatedLogicalS1_ConnectionItem_t *item1 = NULL;

            S1AP_UE_associatedLogicalS1_ConnectionItemResAck_t *ie2 = NULL;
            S1AP_UE_associatedLogicalS1_ConnectionItem_t *item2 = NULL;

            ie1 = (S1AP_UE_associatedLogicalS1_ConnectionItemRes_t *)
                partOfS1_Interface->list.array[i];
            ogs_assert(ie1);

            item1 = &ie1->value.choice.UE_associatedLogicalS1_ConnectionItem;
            ogs_assert(item1);

            if (item1->mME_UE_S1AP_ID == NULL &&
                    item1->eNB_UE_S1AP_ID == NULL) {
                ogs_warn("No MME_UE_S1AP_ID & ENB_UE_S1AP_ID");
                continue;
            }

            ie2 = CALLOC(1,
                    sizeof(S1AP_UE_associatedLogicalS1_ConnectionItemResAck_t));
            ogs_assert(ie2);
            ASN_SEQUENCE_ADD(&list->list, ie2);

            ie2->id =
                S1AP_ProtocolIE_ID_id_UE_associatedLogicalS1_ConnectionItem;
            ie2->criticality = S1AP_Criticality_ignore;
            ie2->value.present = S1AP_UE_associatedLogicalS1_ConnectionItemResAck__value_PR_UE_associatedLogicalS1_ConnectionItem;

            item2 = &ie2->value.choice.UE_associatedLogicalS1_ConnectionItem;
            ogs_assert(item2);

            if (item1->mME_UE_S1AP_ID) {
                item2->mME_UE_S1AP_ID = CALLOC(1,
                        sizeof(S1AP_MME_UE_S1AP_ID_t));
                ogs_assert(item2->mME_UE_S1AP_ID);
                *item2->mME_UE_S1AP_ID = *item1->mME_UE_S1AP_ID;
            }

            if (item1->eNB_UE_S1AP_ID) {
                item2->eNB_UE_S1AP_ID = CALLOC(1,
                        sizeof(S1AP_ENB_UE_S1AP_ID_t));
                ogs_assert(item2->eNB_UE_S1AP_ID);
                *item2->eNB_UE_S1AP_ID = *item1->eNB_UE_S1AP_ID;
            }

            ogs_debug("    MME_UE_S1AP_ID[%d] ENB_UE_S1AP_ID[%d]",
                item2->mME_UE_S1AP_ID ? (int)*item2->mME_UE_S1AP_ID : -1,
                item2->eNB_UE_S1AP_ID ? (int)*item2->eNB_UE_S1AP_ID : -1);
        }
    }

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_write_replace_warning_request(
        ogs_pkbuf_t **s1apbuf, sbc_pws_data_t *sbc_pws)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_WriteReplaceWarningRequest_t *WriteReplaceWarningRequest = NULL;

    S1AP_WriteReplaceWarningRequestIEs_t *ie = NULL;
    S1AP_MessageIdentifier_t *MessageIdentifier = NULL;
    S1AP_SerialNumber_t *SerialNumber = NULL;
    S1AP_RepetitionPeriod_t *RepetitionPeriod = NULL;
    S1AP_NumberofBroadcastRequest_t *NumberofBroadcastRequest = NULL;
    S1AP_DataCodingScheme_t *DataCodingScheme = NULL;
    S1AP_WarningMessageContents_t *WarningMessageContents = NULL;

    ogs_debug("[MME] Write-replace warning request");

    ogs_assert(sbc_pws);

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode = S1AP_ProcedureCode_id_WriteReplaceWarning;
    initiatingMessage->criticality = S1AP_Criticality_reject;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_WriteReplaceWarningRequest;

    WriteReplaceWarningRequest = &initiatingMessage->value.choice.WriteReplaceWarningRequest;

    ie = CALLOC(1, sizeof(S1AP_WriteReplaceWarningRequestIEs_t));
    ASN_SEQUENCE_ADD(&WriteReplaceWarningRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MessageIdentifier;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_WriteReplaceWarningRequestIEs__value_PR_MessageIdentifier;

    MessageIdentifier = &ie->value.choice.MessageIdentifier;

    MessageIdentifier->size = (16 / 8);
    MessageIdentifier->buf = 
        CALLOC(MessageIdentifier->size, sizeof(uint8_t));
    MessageIdentifier->bits_unused = 0;
    MessageIdentifier->buf[0] = (sbc_pws->message_id >> 8) & 0xFF;
    MessageIdentifier->buf[1] = sbc_pws->message_id & 0xFF;

    ie = CALLOC(1, sizeof(S1AP_WriteReplaceWarningRequestIEs_t));
    ASN_SEQUENCE_ADD(&WriteReplaceWarningRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_SerialNumber;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_WriteReplaceWarningRequestIEs__value_PR_SerialNumber;

    SerialNumber = &ie->value.choice.SerialNumber;

    SerialNumber->size = (16 / 8);
    SerialNumber->buf = 
        CALLOC(SerialNumber->size, sizeof(uint8_t));
    SerialNumber->bits_unused = 0;
    SerialNumber->buf[0] = (sbc_pws->serial_number >> 8) & 0xFF;
    SerialNumber->buf[1] = sbc_pws->serial_number & 0xFF;

    /* TODO: optional Warning Area List */

    ie = CALLOC(1, sizeof(S1AP_WriteReplaceWarningRequestIEs_t));
    ASN_SEQUENCE_ADD(&WriteReplaceWarningRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_RepetitionPeriod;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_WriteReplaceWarningRequestIEs__value_PR_RepetitionPeriod;

    RepetitionPeriod = &ie->value.choice.RepetitionPeriod;

    *RepetitionPeriod = sbc_pws->repetition_period;

    /* TODO: optional Extended Repetition Period */

    ie = CALLOC(1, sizeof(S1AP_WriteReplaceWarningRequestIEs_t));
    ASN_SEQUENCE_ADD(&WriteReplaceWarningRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_NumberofBroadcastRequest;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_WriteReplaceWarningRequestIEs__value_PR_NumberofBroadcastRequest;

    NumberofBroadcastRequest = &ie->value.choice.NumberofBroadcastRequest;

    *NumberofBroadcastRequest = sbc_pws->number_of_broadcast;

    /* TODO: optional Warnging Type */

    /* TODO: optional Warning Security Information */

    ie = CALLOC(1, sizeof(S1AP_WriteReplaceWarningRequestIEs_t));
    ASN_SEQUENCE_ADD(&WriteReplaceWarningRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_DataCodingScheme;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_WriteReplaceWarningRequestIEs__value_PR_DataCodingScheme;

    DataCodingScheme = &ie->value.choice.DataCodingScheme;

    DataCodingScheme->size = (8 / 8);
    DataCodingScheme->buf = 
        CALLOC(DataCodingScheme->size, sizeof(uint8_t));
    DataCodingScheme->bits_unused = 0;
    DataCodingScheme->buf[0] = sbc_pws->data_coding_scheme & 0xFF;

    ie = CALLOC(1, sizeof(S1AP_WriteReplaceWarningRequestIEs_t));
    ASN_SEQUENCE_ADD(&WriteReplaceWarningRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_WarningMessageContents;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_WriteReplaceWarningRequestIEs__value_PR_WarningMessageContents;

    WarningMessageContents = &ie->value.choice.WarningMessageContents;

    WarningMessageContents->size = sbc_pws->message_length;;
    WarningMessageContents->buf = 
        CALLOC(WarningMessageContents->size, sizeof(uint8_t));
    memcpy(WarningMessageContents->buf, sbc_pws->message_contents, WarningMessageContents->size);

    /* TODO: optional Concurrent Warning Message Indicator */

    ogs_debug("    Message[%02x,%02x] Serial[%02x,%02x] "
            "Repetition[%d] NumBroadcast[%d]",
        MessageIdentifier->buf[0], MessageIdentifier->buf[1],
        SerialNumber->buf[0], SerialNumber->buf[1],
        (int)*RepetitionPeriod, (int)*NumberofBroadcastRequest);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}

int s1ap_build_kill_request(
        ogs_pkbuf_t **s1apbuf, sbc_pws_data_t *sbc_pws)
{
    int rv;

    S1AP_S1AP_PDU_t pdu;
    S1AP_InitiatingMessage_t *initiatingMessage = NULL;
    S1AP_KillRequest_t *KillRequest = NULL;

    S1AP_KillRequestIEs_t *ie = NULL;
    S1AP_MessageIdentifier_t *MessageIdentifier = NULL;
    S1AP_SerialNumber_t *SerialNumber = NULL;

    ogs_debug("[MME] Kill request");

    ogs_assert(sbc_pws);

    memset(&pdu, 0, sizeof (S1AP_S1AP_PDU_t));
    pdu.present = S1AP_S1AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage = 
        CALLOC(1, sizeof(S1AP_InitiatingMessage_t));

    initiatingMessage = pdu.choice.initiatingMessage;
    initiatingMessage->procedureCode = S1AP_ProcedureCode_id_Kill;
    initiatingMessage->criticality = S1AP_Criticality_reject;
    initiatingMessage->value.present =
        S1AP_InitiatingMessage__value_PR_KillRequest;

    KillRequest = &initiatingMessage->value.choice.KillRequest;

    ie = CALLOC(1, sizeof(S1AP_KillRequestIEs_t));
    ASN_SEQUENCE_ADD(&KillRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_MessageIdentifier;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_KillRequestIEs__value_PR_MessageIdentifier;

    MessageIdentifier = &ie->value.choice.MessageIdentifier;

    MessageIdentifier->size = (16 / 8);
    MessageIdentifier->buf = 
        CALLOC(MessageIdentifier->size, sizeof(uint8_t));
    MessageIdentifier->bits_unused = 0;
    MessageIdentifier->buf[0] = (sbc_pws->message_id >> 8) & 0xFF;
    MessageIdentifier->buf[1] = sbc_pws->message_id & 0xFF;

    ie = CALLOC(1, sizeof(S1AP_KillRequestIEs_t));
    ASN_SEQUENCE_ADD(&KillRequest->protocolIEs, ie);

    ie->id = S1AP_ProtocolIE_ID_id_SerialNumber;
    ie->criticality = S1AP_Criticality_reject;
    ie->value.present = S1AP_KillRequestIEs__value_PR_SerialNumber;

    SerialNumber = &ie->value.choice.SerialNumber;

    SerialNumber->size = (16 / 8);
    SerialNumber->buf = 
        CALLOC(SerialNumber->size, sizeof(uint8_t));
    SerialNumber->bits_unused = 0;
    SerialNumber->buf[0] = (sbc_pws->serial_number >> 8) & 0xFF;
    SerialNumber->buf[1] = sbc_pws->serial_number & 0xFF;

    /* TODO: optional Warning Area List */

    ogs_debug("    Message[%02x,%02x] Serial[%02x,%02x]",
            MessageIdentifier->buf[0], MessageIdentifier->buf[1], 
            SerialNumber->buf[0], SerialNumber->buf[1]);

    rv = s1ap_encode_pdu(s1apbuf, &pdu);
    s1ap_free_pdu(&pdu);

    if (rv != OGS_OK) {
        ogs_error("s1ap_encode_pdu() failed");
        return OGS_ERROR;
    }

    return OGS_OK;
}
