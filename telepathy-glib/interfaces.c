/*
 * telepathy-glib D-Bus interface names and related definitions
 *
 * Copyright © 2007-2012 Collabora Ltd.
 * Copyright © 2007-2009 Nokia Corporation
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

#include <telepathy-glib/interfaces.h>

/* auto-generated implementation stubs */
#include "_gen/interfaces-body.h"

/**
 * SECTION:interfaces
 * @title: Telepathy protocol interface and property names
 * @short_description: D-Bus interface and property names from the
 *  Telepathy spec
 *
 * This header exposes the interface names from the Telepathy specification
 * as cpp defines for strings, such as %TP_IFACE_PROPERTIES_INTERFACE.
 * These are automatically generated from the specification.
 *
 * It also provides related constants such as the common prefix of
 * all connection managers' bus names and object paths.
 *
 * Since 0.7.0 it also provides cpp defines like
 * %TP_IFACE_QUARK_PROPERTIES_INTERFACE, which expand to function calls that
 * return GQuarks for the same strings.
 *
 * Since 0.9.2 it also provides cpp defines like
 * %TP_PROP_CHANNEL_INTERFACE_GROUP_GROUP_FLAGS, which expand to string
 * constants representing fully-qualified D-Bus property names, like
 * <literal>im.telepathy1.Channel.Interface.Group.GroupFlags</literal>.
 *
 * Since 0.11.5 it also provides cpp defines like
 * %TP_TOKEN_CONNECTION_INTERFACE_ALIASING_ALIAS for contact attributes like
 * "im.telepathy1.Connection.Interface.Aliasing/alias",
 * and defines like
 * %TP_TOKEN_CHANNEL_INTERFACE_MEDIA_SIGNALLING_ICE_UDP
 * for handler capability tokens like
 * "im.telepathy1.Channel.Interface.MediaSignalling/gtalk-p2p".
 * (These were present in an incorrect form since 0.11.3.)
 */

/**
 * TP_CM_BUS_NAME_BASE:
 *
 * The prefix for a connection manager's bus name, to which the CM's name
 * (e.g. "gabble") should be appended.
 */

/**
 * TP_CM_OBJECT_PATH_BASE:
 *
 * The prefix for a connection manager's object path, to which the CM's name
 * (e.g. "gabble") should be appended.
 */

/**
 * TP_CONN_BUS_NAME_BASE:
 *
 * The prefix for a connection's bus name, to which the CM's name
 * (e.g. "gabble"), the protocol (e.g. "jabber") and an element or sequence
 * of elements representing the account should be appended.
 */

/**
 * TP_CONN_OBJECT_PATH_BASE:
 *
 * The prefix for a connection's object path, to which the CM's name
 * (e.g. "gabble"), the protocol (e.g. "jabber") and an element or sequence
 * of elements representing the account should be appended.
 */

/**
 * TP_ACCOUNT_MANAGER_BUS_NAME:
 *
 * The account manager's well-known bus name
 */

/**
 * TP_ACCOUNT_MANAGER_OBJECT_PATH:
 *
 * The account manager's standard object path
 */

/**
 * TP_ACCOUNT_OBJECT_PATH_BASE:
 *
 * The common prefix of the object path for all Account objects.
 */

/**
 * TP_CHANNEL_DISPATCHER_BUS_NAME:
 *
 * The channel dispatcher's well-known bus name
 */

/**
 * TP_CHANNEL_DISPATCHER_OBJECT_PATH:
 *
 * The channel dispatcher's standard object path
 */

/**
 * TP_CLIENT_BUS_NAME_BASE:
 *
 * The common prefix of the well-known bus name for any Telepathy Client.
 */

/**
 * TP_CLIENT_OBJECT_PATH_BASE:
 *
 * The common prefix of the well-known object path for any Telepathy Client.
 */

/**
 * TP_DEBUG_OBJECT_PATH:
 *
 * The standard path for objects implementing the Telepathy Debug interface
 * (#TpSvcDebug).
 */
