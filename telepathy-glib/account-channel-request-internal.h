/*<private_header>*/
/*
 * object used to request a channel from a TpAccount (internal)
 *
 * Copyright © 2010-2011 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __TP_ACCOUNT_CHANNEL_REQUEST_INTERNAL_H__
#define __TP_ACCOUNT_CHANNEL_REQUEST_INTERNAL_H__

#include <telepathy-glib/base-client.h>

TpBaseClient * _tp_account_channel_request_get_client (
    TpAccountChannelRequest *self);

#endif
