/*
 * defs.h - miscellaneous definitions
 *
 * Copyright (C) 2007 Collabora Ltd.
 * Copyright (C) 2007 Nokia Corporation
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

G_END_DECLS
#endif
