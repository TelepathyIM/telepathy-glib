/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __TPL_LOG_MANAGER_PRIV_H__
#define __TPL_LOG_MANAGER_PRIV_H__

#include <telepathy-logger/log-manager.h>
#include <telepathy-logger/log-store-factory.h>

gboolean tpl_log_manager_add_message (TplLogManager *manager,
    TplLogEntry *message, GError **error);

gboolean tpl_log_manager_add_message_async_finish (GAsyncResult *result,
    GError **error);

void tpl_log_manager_add_message_async (TplLogManager *manager,
    TplLogEntry *message, GAsyncReadyCallback callback, gpointer user_data);

#endif /* __TPL_LOG_MANAGER_PRIV_H__ */
