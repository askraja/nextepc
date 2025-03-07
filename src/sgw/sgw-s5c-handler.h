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

#ifndef SGW_S5C_HANDLER_H
#define SGW_S5C_HANDLER_H

#include "gtp/gtp-message.h"

#include "sgw-context.h"

#ifdef __cplusplus
extern "C" {
#endif

void sgw_s5c_handle_create_session_response(gtp_xact_t *s5c_xact,
        sgw_sess_t *sess, gtp_message_t *gtp_message);
void sgw_s5c_handle_delete_session_response(gtp_xact_t *s5c_xact,
        sgw_sess_t *sess, gtp_message_t *gtp_message);
void sgw_s5c_handle_create_bearer_request(gtp_xact_t *s5c_xact,
        sgw_sess_t *sess, gtp_message_t *gtp_message);
void sgw_s5c_handle_update_bearer_request(gtp_xact_t *s5c_xact, 
        sgw_sess_t *sess, gtp_message_t *gtp_message);
void sgw_s5c_handle_delete_bearer_request(gtp_xact_t *s5c_xact, 
        sgw_sess_t *sess, gtp_message_t *gtp_message);

#ifdef __cplusplus
}
#endif

#endif /* SGW_S5C_HANDLER_H */
