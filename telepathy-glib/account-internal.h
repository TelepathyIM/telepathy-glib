/*
 * TpAccount - proxy for a Telepathy account (internals)
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
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

#ifndef TP_ACCOUNT_INTERNAL_H
#define TP_ACCOUNT_INTERNAL_H

#include <telepathy-glib/account.h>

G_BEGIN_DECLS

const GQuark * _tp_account_get_requested_features (TpAccount *account);

const GQuark * _tp_account_get_actual_features (TpAccount *account);

const GQuark * _tp_account_get_missing_features (TpAccount *account);

void _tp_account_refresh_properties (TpAccount *account);

G_END_DECLS

#endif
