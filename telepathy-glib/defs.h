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
 * TP_CM_BUS_NAME_BASE:
 *
 * The prefix for a connection manager's bus name, to which the CM's name
 * (e.g. "gabble") should be appended.
 */
#define TP_CM_BUS_NAME_BASE    "org.freedesktop.Telepathy.ConnectionManager."

/**
 * TP_CM_OBJECT_PATH_BASE:
 *
 * The prefix for a connection manager's object path, to which the CM's name
 * (e.g. "gabble") should be appended.
 */
#define TP_CM_OBJECT_PATH_BASE "/org/freedesktop/Telepathy/ConnectionManager/"

/**
 * TP_CONN_BUS_NAME_BASE:
 *
 * The prefix for a connection's bus name, to which the CM's name
 * (e.g. "gabble"), the protocol (e.g. "jabber") and an element or sequence
 * of elements representing the account should be appended.
 */
#define TP_CONN_BUS_NAME_BASE "org.freedesktop.Telepathy.Connection."

/**
 * TP_CONN_OBJECT_PATH_BASE:
 *
 * The prefix for a connection's object path, to which the CM's name
 * (e.g. "gabble"), the protocol (e.g. "jabber") and an element or sequence
 * of elements representing the account should be appended.
 */
#define TP_CONN_OBJECT_PATH_BASE "/org/freedesktop/Telepathy/Connection/"

/**
 * TP_ACCOUNT_MANAGER_BUS_NAME:
 *
 * The account manager's well-known bus name
 */
#define TP_ACCOUNT_MANAGER_BUS_NAME "org.freedesktop.Telepathy.AccountManager"

/**
 * TP_ACCOUNT_MANAGER_OBJECT_PATH:
 *
 * The account manager's standard object path
 */
#define TP_ACCOUNT_MANAGER_OBJECT_PATH "/org/freedesktop/Telepathy/AccountManager"

/**
 * TP_ACCOUNT_OBJECT_PATH_BASE:
 *
 * The common prefix of the object path for all Account objects.
 */
#define TP_ACCOUNT_OBJECT_PATH_BASE "/org/freedesktop/Telepathy/Account/"

/**
 * TP_CHANNEL_DISPATCHER_BUS_NAME:
 *
 * The channel dispatcher's well-known bus name
 */
#define TP_CHANNEL_DISPATCHER_BUS_NAME "org.freedesktop.Telepathy.ChannelDispatcher"

/**
 * TP_CHANNEL_DISPATCHER_OBJECT_PATH:
 *
 * The channel dispatcher's standard object path
 */
#define TP_CHANNEL_DISPATCHER_OBJECT_PATH "/org/freedesktop/Telepathy/ChannelDispatcher"

/**
 * TP_CLIENT_BUS_NAME_BASE:
 *
 * The common prefix of the well-known bus name for any Telepathy Client.
 */
#define TP_CLIENT_BUS_NAME_BASE "org.freedesktop.Telepathy.Client."

/**
 * TP_CLIENT_OBJECT_PATH_BASE:
 *
 * The common prefix of the well-known object path for any Telepathy Client.
 */
#define TP_CLIENT_OBJECT_PATH_BASE "/org/freedesktop/Telepathy/Client/"

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
 * concept as %GDK_CURRENT_TIME in Gdk (but note that the numerical value used
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
#else
#define _TP_GNUC_DEPRECATED G_GNUC_DEPRECATED
#endif

G_END_DECLS
#endif
