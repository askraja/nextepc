
#include <mongoc.h>
#include "core/abts.h"
#include "base/types.h"
#include "asn1c/s1ap-message.h"
#include "app/context.h"
#include "test-packet.h"
#include "pcscf-fd-path.h"

static void volte_test1(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *s1ap;
    ogs_pkbuf_t *sendbuf;
    ogs_pkbuf_t *recvbuf;
    s1ap_message_t message;
    int i;
    int msgindex = 0;
    uint8_t *rx_sid = NULL;

    mongoc_collection_t *collection = NULL;
    bson_t *doc = NULL;
    int64_t count = 0;
    bson_error_t error;
    const char *json =
    "{"
      "\"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd2b\" }," 
      "\"imsi\" : \"001010123456819\","
      "\"pdn\" : ["
      "{ \"apn\" : \"internet\","
        "\"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd32\" },"
        "\"qos\" : {"
          "\"qci\" : 9,"
          "\"arp\" : {"
            "\"priority_level\" : 8,"
            "\"pre_emption_vulnerability\" : 1,"
            "\"pre_emption_capability\" : 1 } },"
        "\"type\" : 2 },"
      "{ \"apn\" : \"internet.ng2.mnet\","
        "\"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd2c\" },"
        "\"pcc_rule\" : ["
        "{ \"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd2d\" },"
          "\"qos\" : {"
            "\"qci\" : 1,"
            "\"gbr\" : {"
              "\"downlink\" : { \"$numberLong\" : \"64\" },"
              "\"uplink\" : { \"$numberLong\" : \"44\" } },"
            "\"mbr\" : {"
              "\"downlink\" : { \"$numberLong\" : \"64\" },"
              "\"uplink\" : { \"$numberLong\" : \"44\" } },"
            "\"arp\" : {"
              "\"priority_level\" : 3,"
              "\"pre_emption_vulnerability\" : 0,"
              "\"pre_emption_capability\" : 0 } }"
        "} ],"
        "\"ambr\" : {"
          "\"downlink\" : { \"$numberLong\" : \"35840\" },"
          "\"uplink\" : { \"$numberLong\" : \"15360\" } },"
        "\"qos\" : {"
          "\"qci\" : 6,"
          "\"arp\" : {"
            "\"priority_level\" : 6,"
            "\"pre_emption_vulnerability\" : 1,"
            "\"pre_emption_capability\" : 1 } },"
        "\"type\" : 2 }"
      "],"
      "\"ambr\" : {"
        "\"downlink\" : { \"$numberLong\" : \"1024000\" },"
        "\"uplink\" : { \"$numberLong\" : \"1024000\" } },"
      "\"subscribed_rau_tau_timer\" : 12,"
      "\"network_access_mode\" : 2,"
      "\"subscriber_status\" : 0,"
      "\"access_restriction_data\" : 32,"
      "\"security\" : {"
          "\"k\" : \"465B5CE8 B199B49F AA5F0A2E E238A6BC\","
          "\"op\" : \"5F1D289C 5D354D0A 140C2548 F5F3E3BA\","
          "\"amf\" : \"8000\","
          "\"sqn\" : { \"$numberLong\" : \"64\" }, "
          "\"rand\" : \"20080C38 18183B52 2614162C 07601D0D\" }, "
      "\"__v\" : 0"
    "}";

    /* eNB connects to MME */
    s1ap = testenb_s1ap_client("127.0.0.1");
    ABTS_PTR_NOTNULL(tc, s1ap);

    /* Send S1-Setup Reqeust */
    rv = tests1ap_build_setup_req(
            &sendbuf, S1AP_ENB_ID_PR_macroENB_ID, 0x54f64, 12345, 1, 1, 2);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive S1-Setup Response */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    rv = s1ap_decode_pdu(&message, recvbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    s1ap_free_pdu(&message);
    ogs_pkbuf_free(recvbuf);

    collection = mongoc_client_get_collection(
        context_self()->db.client,
        context_self()->db.name, "subscribers");
    ABTS_PTR_NOTNULL(tc, collection);

    doc = bson_new_from_json((const uint8_t *)json, -1, &error);;
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_TRUE(tc, mongoc_collection_insert(collection, 
                MONGOC_INSERT_NONE, doc, NULL, &error));
    bson_destroy(doc);

    doc = BCON_NEW("imsi", BCON_UTF8("001010123456819"));
    ABTS_PTR_NOTNULL(tc, doc);
    do
    {
        count = mongoc_collection_count (
            collection, MONGOC_QUERY_NONE, doc, 0, 0, NULL, &error);
    } while (count == 0);
    bson_destroy(doc);

    /***********************************************************************
     * Attach Request : Known IMSI, Integrity Protected, No Security Context
     * Send Initial-UE Message + Attach Request + PDN Connectivity        */
    rv = tests1ap_build_initial_ue_msg(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication Request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send Authentication Response */
    rv = tests1ap_build_authentication_response(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode Command */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send Security mode Complete */
    rv = tests1ap_build_security_mode_complete(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive ESM Information Request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send ESM Information Response */
    rv = tests1ap_build_esm_information_response(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Initial Context Setup Request + 
     * Attach Accept + 
     * Activate Default Bearer Context Request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send UE Capability Info Indication */
    rv = tests1ap_build_ue_capability_info_indication(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Initial Context Setup Response */
    rv = tests1ap_build_initial_context_setup_response(&sendbuf,
            1, 1, 5, 1, "127.0.0.5");
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Attach Complete + Activate default EPS bearer cotext accept */
    rv = tests1ap_build_attach_complete(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive EMM information */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send PDN Connectivity Request */
    rv = tests1ap_build_pdn_connectivity_request(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive E-RAB Setup Request +
     * Activate default EPS bearer context request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send E-RAB Setup Response */
    rv = tests1ap_build_e_rab_setup_response(&sendbuf, 1, 1, 6, 2, "127.0.0.5");
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Activate default EPS bearer context accept */
    rv = tests1ap_build_activate_default_bearer_accept(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send AA-Request */
    pcscf_rx_send_aar(&rx_sid, "45.45.0.3", 1, 1);

    /* Receive E-RAB Setup Request +
     * Activate dedicated EPS bearer context request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send E-RAB Setup Response */
    rv = tests1ap_build_e_rab_setup_response(&sendbuf, 1, 1, 7, 3, "127.0.0.5");
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Activate dedicated EPS bearer context accept */
    rv = tests1ap_build_activate_dedicated_bearer_accept(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send AA-Request without Flow */
    pcscf_rx_send_aar(&rx_sid, "45.45.0.3", 2, 1);

    /* Receive E-RAB Modify Request +
     * Modify EPS bearer context request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send E-RAB Modify Response */
    rv = tests1ap_build_e_rab_modify_response(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Modify EPS bearer context accept */
    rv = tests1ap_build_modify_bearer_accept(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Session-Termination-Request */
    pcscf_rx_send_str(rx_sid);

    /* Receive E-RAB Release Command +
     * Dectivate EPS bearer context request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send E-RAB Release Response */
    rv = tests1ap_build_e_rab_release_response(&sendbuf, msgindex+1);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Deactivate EPS bearer context accept */
    rv = tests1ap_build_deactivate_bearer_accept(&sendbuf, msgindex+1);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Detach Request */
    rv = tests1ap_build_detach_request(&sendbuf, msgindex+1);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UE Context Release Command */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send UE Context Release Complete */
    rv = tests1ap_build_ue_context_release_complete(&sendbuf, msgindex+1);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    /********** Remove Subscriber in Database */
    doc = BCON_NEW("imsi", BCON_UTF8("001010123456819"));
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_TRUE(tc, mongoc_collection_remove(collection, 
            MONGOC_REMOVE_SINGLE_REMOVE, doc, NULL, &error)) 
    bson_destroy(doc);

    mongoc_collection_destroy(collection);

    /* eNB disonncect from MME */
    testenb_s1ap_close(s1ap);
}

static void volte_test2(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *s1ap;
    ogs_pkbuf_t *sendbuf;
    ogs_pkbuf_t *recvbuf;
    s1ap_message_t message;
    int i;
    int msgindex = 0;
    uint8_t *rx_sid = NULL;

    mongoc_collection_t *collection = NULL;
    bson_t *doc = NULL;
    int64_t count = 0;
    bson_error_t error;
    const char *json =
    "{"
      "\"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd2b\" }," 
      "\"imsi\" : \"001010123456819\","
      "\"pdn\" : ["
      "{ \"apn\" : \"internet\","
        "\"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd32\" },"
        "\"qos\" : {"
          "\"qci\" : 9,"
          "\"arp\" : {"
            "\"priority_level\" : 8,"
            "\"pre_emption_vulnerability\" : 1,"
            "\"pre_emption_capability\" : 1 } },"
        "\"type\" : 2 },"
      "{ \"apn\" : \"internet.ng2.mnet\","
        "\"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd2c\" },"
        "\"pcc_rule\" : ["
        "{ \"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd2d\" },"
          "\"qos\" : {"
            "\"qci\" : 1,"
            "\"gbr\" : {"
              "\"downlink\" : { \"$numberLong\" : \"64\" },"
              "\"uplink\" : { \"$numberLong\" : \"44\" } },"
            "\"mbr\" : {"
              "\"downlink\" : { \"$numberLong\" : \"64\" },"
              "\"uplink\" : { \"$numberLong\" : \"44\" } },"
            "\"arp\" : {"
              "\"priority_level\" : 3,"
              "\"pre_emption_vulnerability\" : 0,"
              "\"pre_emption_capability\" : 0 } },"
          "\"flow\" : ["
          "{ \"direction\" : 2,"
            "\"description\" : \"permit out udp from any 1-65535 to 10.200.136.98/32 23454\","
            "\"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd31\" } },"
          "{ \"direction\" : 1,"
            "\"description\" : \"permit out udp from any 50020 to 10.200.136.98/32 1-65535\","
            "\"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd30\" } },"
          "{ \"direction\" : 2,"
            "\"description\" : \"permit out udp from any 1-65535 to 10.200.136.98/32 23455\","
            "\"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd2f\" } },"
          "{ \"direction\" : 1,"
            "\"description\" : \"permit out udp from any 50021 to 10.200.136.98/32 1-65535\","
            "\"_id\" : { \"$oid\" : \"599eb929c850caabcbfdcd2e\" } } ]"
        "} ],"
        "\"ambr\" : {"
          "\"downlink\" : { \"$numberLong\" : \"35840\" },"
          "\"uplink\" : { \"$numberLong\" : \"15360\" } },"
        "\"qos\" : {"
          "\"qci\" : 6,"
          "\"arp\" : {"
            "\"priority_level\" : 6,"
            "\"pre_emption_vulnerability\" : 1,"
            "\"pre_emption_capability\" : 1 } },"
        "\"type\" : 2 }"
      "],"
      "\"ambr\" : {"
        "\"downlink\" : { \"$numberLong\" : \"1024000\" },"
        "\"uplink\" : { \"$numberLong\" : \"1024000\" } },"
      "\"subscribed_rau_tau_timer\" : 12,"
      "\"network_access_mode\" : 2,"
      "\"subscriber_status\" : 0,"
      "\"access_restriction_data\" : 32,"
      "\"security\" : {"
          "\"k\" : \"465B5CE8 B199B49F AA5F0A2E E238A6BC\","
          "\"op\" : \"5F1D289C 5D354D0A 140C2548 F5F3E3BA\","
          "\"amf\" : \"8000\","
          "\"sqn\" : { \"$numberLong\" : \"64\" }, "
          "\"rand\" : \"20080C38 18183B52 2614162C 07601D0D\" }, "
      "\"__v\" : 0"
    "}";

    /* eNB connects to MME */
    s1ap = testenb_s1ap_client("127.0.0.1");
    ABTS_PTR_NOTNULL(tc, s1ap);

    /* Send S1-Setup Reqeust */
    rv = tests1ap_build_setup_req(
            &sendbuf, S1AP_ENB_ID_PR_macroENB_ID, 0x54f64, 12345, 1, 1, 2);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive S1-Setup Response */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    rv = s1ap_decode_pdu(&message, recvbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    s1ap_free_pdu(&message);
    ogs_pkbuf_free(recvbuf);

    collection = mongoc_client_get_collection(
        context_self()->db.client,
        context_self()->db.name, "subscribers");
    ABTS_PTR_NOTNULL(tc, collection);

    doc = bson_new_from_json((const uint8_t *)json, -1, &error);;
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_TRUE(tc, mongoc_collection_insert(collection, 
                MONGOC_INSERT_NONE, doc, NULL, &error));
    bson_destroy(doc);

    doc = BCON_NEW("imsi", BCON_UTF8("001010123456819"));
    ABTS_PTR_NOTNULL(tc, doc);
    do
    {
        count = mongoc_collection_count (
            collection, MONGOC_QUERY_NONE, doc, 0, 0, NULL, &error);
    } while (count == 0);
    bson_destroy(doc);

    /***********************************************************************
     * Attach Request : Known IMSI, Integrity Protected, No Security Context
     * Send Initial-UE Message + Attach Request + PDN Connectivity        */
    rv = tests1ap_build_initial_ue_msg(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication Request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send Authentication Response */
    rv = tests1ap_build_authentication_response(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode Command */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send Security mode Complete */
    rv = tests1ap_build_security_mode_complete(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive ESM Information Request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send ESM Information Response */
    rv = tests1ap_build_esm_information_response(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Initial Context Setup Request + 
     * Attach Accept + 
     * Activate Default Bearer Context Request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send UE Capability Info Indication */
    rv = tests1ap_build_ue_capability_info_indication(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Initial Context Setup Response */
    rv = tests1ap_build_initial_context_setup_response(&sendbuf,
            1, 1, 5, 1, "127.0.0.5");
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Attach Complete + Activate default EPS bearer cotext accept */
    rv = tests1ap_build_attach_complete(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive EMM information */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send PDN Connectivity Request */
    rv = tests1ap_build_pdn_connectivity_request(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive E-RAB Setup Request +
     * Activate default EPS bearer context request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send E-RAB Setup Response */
    rv = tests1ap_build_e_rab_setup_response(&sendbuf, 1, 1, 6, 2, "127.0.0.5");
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Activate default EPS bearer context accept */
    rv = tests1ap_build_activate_default_bearer_accept(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive E-RAB Setup Request +
     * Activate dedicated EPS bearer context request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send Activate dedicated EPS bearer context accept */
    rv = tests1ap_build_activate_dedicated_bearer_accept(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send E-RAB Setup Response */
    rv = tests1ap_build_e_rab_setup_response(&sendbuf, 1, 1, 7, 3, "127.0.0.5");
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send AA-Request */
    pcscf_rx_send_aar(&rx_sid, "45.45.0.5", 0, 1);

    /* Receive downlink NAS transport +
     * Modify EPS bearer context request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send Modify EPS bearer context accept */
    rv = tests1ap_build_modify_bearer_accept(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDN disconnectivity request */
    rv = tests1ap_build_pdn_disconnectivity_request(&sendbuf, msgindex+1);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive E-RAB Release Command +
     * Deactivate EPS bearer context request */
    recvbuf = testenb_s1ap_read(s1ap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send E-RAB Release Response */
    rv = tests1ap_build_e_rab_release_response(&sendbuf, msgindex);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Deactivate EPS bearer context accept */
    rv = tests1ap_build_deactivate_bearer_accept(&sendbuf, msgindex+2);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    rv = testenb_s1ap_send(s1ap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    /********** Remove Subscriber in Database */
    doc = BCON_NEW("imsi", BCON_UTF8("001010123456819"));
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_TRUE(tc, mongoc_collection_remove(collection, 
            MONGOC_REMOVE_SINGLE_REMOVE, doc, NULL, &error)) 
    bson_destroy(doc);

    mongoc_collection_destroy(collection);

    /* eNB disonncect from MME */
    testenb_s1ap_close(s1ap);
}

abts_suite *test_volte(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, volte_test1, NULL);
    abts_run_test(suite, volte_test2, NULL);

    return suite;
}
