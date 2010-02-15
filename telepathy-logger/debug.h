/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
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
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#ifndef __TPL_DEBUG_H__
#define __TPL_DEBUG_H__

#include "config.h"

#include <glib.h>

#ifdef ENABLE_DEBUG

G_BEGIN_DECLS

typedef enum
{
  TPL_DEBUG_ACTION_CHAIN  = 1 << 0,
  TPL_DEBUG_CONF          = 1 << 1,
  TPL_DEBUG_CONTACT       = 1 << 2,
  TPL_DEBUG_CHANNEL       = 1 << 3,
  TPL_DEBUG_DBUS_SERVICE  = 1 << 4,
  TPL_DEBUG_LOG_ENTRY     = 1 << 5,
  TPL_DEBUG_LOG_MANAGER   = 1 << 6,
  TPL_DEBUG_LOG_STORE     = 1 << 7,
  TPL_DEBUG_MAIN          = 1 << 8,
  TPL_DEBUG_OBSERVER      = 1 << 9
} TplDebugFlags;

void tpl_debug_set_flags_from_env (void);
void tpl_debug_set_flags (TplDebugFlags flags);
gboolean tpl_debug_flag_is_set (TplDebugFlags flag);
void tpl_debug_free (void);
void tpl_debug (TplDebugFlags flag, const gchar *format, ...)
    G_GNUC_PRINTF (2, 3);


G_END_DECLS

#ifdef DEBUG_FLAG

#define DEBUG(format, ...) \
  tpl_debug (DEBUG_FLAG, "%s: " format, G_STRFUNC, ##__VA_ARGS__)

#define DEBUGGING gabble_debug_flag_is_set (DEBUG_FLAG)

/* The same of DEBUG, printing also the object-path property for the TpProxy
 * passed as first arg */
#define PATH_DEBUG(proxy, format, ...) \
G_STMT_START { \
  const gchar *path; \
  g_assert (TP_IS_PROXY (proxy)); \
  path = tp_proxy_get_object_path (TP_PROXY (proxy)); \
  DEBUG (" %s: " format, path, ##__VA_ARGS__); \
} G_STMT_END

#endif /* DEBUG_FLAG */

#else /* ENABLE_DEBUG */

#ifdef DEBUG_FLAG

#define DEBUG(format, ...) G_STMT_START { } G_STMT_END
#define DEBUGGING 0
#define PATH_DEBUG(chan, format, ...) G_STMT_START { } G_STMT_END

#endif /* DEBUG_FLAG */

#define tpl_debug_free() G_STMT_START { } G_STMT_END

#endif /* ENABLE_DEBUG */

#endif /* __TPL_DEBUG_H__ */
