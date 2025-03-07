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
#include "nas-path.h"
#include "s1ap-path.h"

#include "mme-sm.h"
#include "mme-s6a-handler.h"

void mme_s6a_handle_aia(mme_ue_t *mme_ue, s6a_aia_message_t *aia_message)
{
    int rv;
    e_utran_vector_t *e_utran_vector = NULL;

    ogs_assert(mme_ue);
    ogs_assert(aia_message);
    e_utran_vector = &aia_message->e_utran_vector;
    ogs_assert(e_utran_vector);

    mme_ue->xres_len = e_utran_vector->xres_len;
    memcpy(mme_ue->xres, e_utran_vector->xres, mme_ue->xres_len);
    memcpy(mme_ue->kasme, e_utran_vector->kasme, OGS_SHA256_DIGEST_SIZE);
    memcpy(mme_ue->rand, e_utran_vector->rand, RAND_LEN);

    CLEAR_MME_UE_TIMER(mme_ue->t3460);

    rv = nas_send_authentication_request(mme_ue, e_utran_vector);
    ogs_assert(rv == OGS_OK);
}

void mme_s6a_handle_ula(mme_ue_t *mme_ue, s6a_ula_message_t *ula_message)
{
    s6a_subscription_data_t *subscription_data = NULL;

    ogs_assert(mme_ue);
    ogs_assert(ula_message);
    subscription_data = &ula_message->subscription_data;
    ogs_assert(subscription_data);

    memcpy(&mme_ue->subscription_data,
            subscription_data, sizeof(s6a_subscription_data_t));
}
