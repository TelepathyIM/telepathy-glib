/*
 * defs.h - miscellaneous definitions
 *
 * Copyright (C) 2007-2009 Collabora Ltd.
 * Copyright (C) 2007-2009 Nokia Corporation
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

#ifndef __TP_DEFS_H__
#define __TP_DEFS_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * TP_USER_ACTION_TIME_NOT_USER_ACTION:
 *
 * The "user action time" used by methods like
 * tp_account_channel_request_new() to represent channel requests that are
 * not a result of user action.
 *
 * See also #TpAccountChannelRequest:user-action-time,
 * tp_user_action_time_from_x11(), tp_user_action_time_should_present() and
 * %TP_USER_ACTION_TIME_CURRENT_TIME.
 *
 * Since: 0.11.13
 */
#define TP_USER_ACTION_TIME_NOT_USER_ACTION (G_GINT64_CONSTANT (0))

/**
 * TP_USER_ACTION_TIME_CURRENT_TIME:
 *
 * The "user action time" used by methods like
 * tp_account_channel_request_new() to represent channel requests that should
 * be treated as though they happened at the current time. This is the same
 * concept as %GDK_CURRENT_TIME in GDK (but note that the numerical value used
 * in Telepathy is not the same).
 *
 * See also #TpAccountChannelRequest:user-action-time,
 * tp_user_action_time_from_x11(), tp_user_action_time_should_present() and
 * %TP_USER_ACTION_TIME_NOT_USER_ACTION.
 *
 * Since: 0.11.13
 */
#define TP_USER_ACTION_TIME_CURRENT_TIME (G_MAXINT64)

/* telepathy-glib-specific version of G_GNUC_DEPRECATED so our regression
 * tests can continue to test deprecated functionality, while avoiding
 * deprecated bits of other libraries */
#ifdef _TP_IGNORE_DEPRECATIONS
#define _TP_GNUC_DEPRECATED /* nothing */
#define _TP_GNUC_DEPRECATED_FOR(f) /* nothing */
#else
#define _TP_GNUC_DEPRECATED G_GNUC_DEPRECATED
#define _TP_GNUC_DEPRECATED_FOR(f) G_GNUC_DEPRECATED_FOR(f)
#endif

G_END_DECLS
#endif
