/* This file contains no code - it's just here for gtkdoc to pick up
 * documentation for otherwise undocumented generated files.
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

/**
 * SECTION:svc-generic
 * @title: Generic service-side interfaces
 * @short_description: GInterfaces for D-Bus objects exporting Telepathy
 *  properties and common D-Bus core interfaces
 * @see_also: #TpPropertiesMixin
 *
 * The D-Bus Properties interface associates named properties with any D-Bus
 * object.
 *
 * The D-Bus Introspectable interface provides introspection information.
 *
 * The D-Bus Peer interface is exported by every D-Bus object.
 *
 * The Telepathy Properties interface associates a number of named properties
 * with a channel, connection or other D-Bus object. Signals are emitted
 * when the properties or their flags (readable/writable) change.
 */

/**
 * SECTION:svc-channel
 * @title: Service-side Channel types and interfaces
 * @short_description: GInterfaces for Telepathy Channel objects
 * @see_also: #TpChannelIface
 *
 * These interfaces (auto-generated from the Telepathy spec) make it easier
 * to export objects implementing the Telepathy Channel and its types and
 * optional interfaces, with the correct method and signal signatures,
 * and emit signals from those objects in a type-safe way.
 */

/**
 * SECTION:svc-connection
 * @title: Service-side Connection interfaces
 * @short_description: GInterfaces for Telepathy Connection objects
 * @see_also: #TpBaseConnection
 *
 * These interfaces (auto-generated from the Telepathy spec) make it easier
 * to export objects implementing the Telepathy Connection and its
 * optional interfaces, with the correct method and signal signatures,
 * and emit signals from those objects in a type-safe way.
 */

/**
 * SECTION:svc-connection-manager
 * @title: Service-side Connection Manager interface
 * @short_description: GInterface for Telepathy ConnectionManager objects
 * @see_also: #TpBaseConnection
 *
 * The #TpSvcConnectionManager interface (auto-generated from the Telepathy
 * spec) makes it easier to export an object implementing the Telepathy
 * ConnectionManager interface, with the correct method and signal signatures,
 * and emit signals from that object in a type-safe way.
 */

/**
 * SECTION:svc-media-interfaces
 * @title: Service-side media streaming helper interfaces
 * @short_description: media session and media stream
 * @see_also: #TpSvcChannelTypeStreamedMedia
 *
 * These interfaces (auto-generated from the telepathy spec) make it easier
 * to export the objects used to implement #TpSvcChannelTypeStreamedMedia,
 * with the correct method and signal signatures, and emit signals from those
 * objects.
 */

/**
 * SECTION:defs
 * @title: Miscellaneous definitions
 * @short_description: Definitions useful for working with the Telepathy
 *   protocol
 *
 * This header contains definitions which didn't fit into enums.h,
 * interfaces.h or errors.h.
 *
 * Since: 0.7.0. In older versions, these constants were in base-connection.h
 * and base-connection-manager.h.
 */

/**
 * SECTION:enums
 * @title: Telepathy protocol enumerations
 * @short_description: Enumerated types and bitfields from the Telepathy spec
 *
 * This header exposes the constants from the Telepathy specification as
 * C enums. It is automatically generated from the specification.
 *
 * The names used in the specification (e.g.
 * Connection_Status_Connected) are converted to upper-case and given a
 * TP_ prefix, e.g. TP_CONNECTION_STATUS_CONNECTED.
 *
 * Each enum also has a constant for the number of members, named like
 * NUM_TP_CONNECTION_STATUSES. The pluralization is currently hard-coded
 * in the conversion scripts, but should move into the specification
 * in future.
 *
 * Constants LAST_TP_CONNECTION_STATUS, etc. are also provided. These are
 * deprecated and will be removed in a future release.
 */

/**
 * SECTION:interfaces
 * @title: Telepathy protocol interface strings
 * @short_description: D-Bus interface names from the Telepathy spec
 *
 * This header exposes the interface names from the Telepathy specification
 * as cpp defines for strings, such as %TP_IFACE_PROPERTIES_INTERFACE.
 * It is automatically generated from the specification.
 *
 * Since 0.7.0 it also provides cpp defines like
 * %TP_IFACE_QUARK_PROPERTIES_INTERFACE, which expand to function calls that
 * return GQuarks for the same strings.
 */

/**
 * SECTION:errors
 * @title: Telepathy protocol errors
 * @short_description: The errors from the Telepathy D-Bus spec, as a
 *  GLib error domain
 *
 * This header provides the Telepathy D-Bus errors, in the form of a
 * GLib error domain. For D-Bus methods which fail with one of these errors,
 * dbus-glib will generate a reply message with the appropriate error.
 *
 * It also provides utility functions used by functions which return an error.
 */

/**
 * SECTION:handle
 * @title: TpHandle
 * @short_description: type representing handles
 * @see_also: TpHandleRepoIface
 *
 * The TpHandle type represents a Telepathy handle.
 */

/**
 * SECTION:channel-group
 * @title: Group interface on Channels
 * @short_description: client-side wrappers for the Group interface
 * @see_also: #TpChannel
 *
 * Many Telepathy Channel objects can be seen as representing groups or
 * sets of contacts. The Telepathy specification represents this by a common
 * interface, Group. This section documents the auto-generated C wrappers for
 * the Group interface.
 *
 * Contacts can be in four states:
 *
 * * in the group (the "members" set)
 *
 * * "local pending" (waiting to be added to the group by the local client
 *   calling AddMembers())
 *
 * * "remote pending" (waiting to be added to the group by some other
 *   action, probably by someone else)
 *
 * * no relationship with the group at all
 *
 * For instance, chatrooms implement the Group interface. Contacts in the
 * chatroom are members, and contacts who we've invited to the group, or
 * contacts who've requested permission to join, are remote pending. If the
 * local user has been invited by another contact, they will appear in the
 * local-pending set until they accept or decline the invitation.
 */

/**
 * SECTION:channel-text
 * @title: Text channels
 * @short_description: client-side wrappers for the Text channel type, and
 *  the Chat State and Password interfaces
 * @see_also: channel-group, #TpChannel
 *
 * A major use for instant messaging is obviously to send messages.
 * Channels of type Text represent IM conversations or chat rooms.
 *
 * This section documents the auto-generated C wrappers for the Text channel
 * type, and also for the Chat State and Password interfaces, which are
 * usually used in conjunction with Text channels.
 */

/**
 * SECTION:channel-media
 * @title: Media channels
 * @short_description: client-side wrappers for the Streamed Media channel
 *  type, and the DTMF and Media Signalling interfaces
 * @see_also: channel-group, #TpChannel
 *
 * This section documents the auto-generated C wrappers for the Streamed Media
 * channel type, and the DTMF and Media Signalling interfaces which are
 * optionally supported by channels of this type.
 *
 * Streamed Media channels represent real-time audio or video streaming,
 * including voice over IP, webcams, and telephony.
 *
 * Channels of type Streamed Media may support the Media Signalling interface.
 * If not, the connection manager is assumed to be presenting the media
 * streams to the user automatically (for instance, in a connection manager
 * like gnome-phone-manager or telepathy-snom that remotely controls a
 * telephone, the phone's own speaker and microphone will probably be
 * used directly).
 *
 * If Media Signalling is supported, the Telepathy client is responsible for
 * actually streaming the media, using the Media Signalling interface to
 * provide signalling (connection managers might implement this interface in
 * terms of Jingle or SDP, for instance). The Telepathy project suggests that
 * client authors use the Farsight library for this; the glue between Media
 * Signalling and Farsight is currently done in telepathy-stream-engine, an
 * additional D-Bus service, but it will be provided as a library in future.
 *
 * Channels of type Streamed Media may also support the DTMF interface.
 */

/**
 * SECTION:channel-tubes
 * @title: Tubes channels
 * @short_description: client-side wrappers for the Tubes channel type
 * @see_also: channel-group
 *
 * A "tube" is a mechanism for arbitrary data transfer.
 * This section documents the auto-generated C wrappers for the Tubes
 * channel type.
 */

/**
 * SECTION:channel-roomlist
 * @title: Room List channels
 * @short_description: client-side wrappers for the Room List channel type
 * @see_also: #TpChannel
 *
 * Many instant messaging protocols allow named chatrooms to be listed.
 * This section documents the auto-generated C wrappers for the Room List
 * channel type.
 */

/**
 * SECTION:connection-avatars
 * @title: Connection Avatars interface
 * @short_description: client-side wrappers for the Avatars interface
 * @see_also: #TpConnection
 *
 * Most instant messaging protocols allow users to set an icon or avatar.
 * This section documents the auto-generated C wrappers for the Avatar
 * interface, used with #TpConnection objects.
 */

/**
 * SECTION:connection-aliasing
 * @title: Connection Aliasing interface
 * @short_description: client-side wrappers for the Aliasing interface
 * @see_also: #TpConnection
 *
 * Most instant messaging protocols allow users to set a nickname or
 * alias. This section documents the auto-generated C wrappers for the
 * Aliasing interface, used with #TpConnection objects.
 */

/**
 * SECTION:connection-caps
 * @title: Connection Capabilities interface
 * @short_description: client-side wrappers for the Capabilities interface
 * @see_also: #TpConnection
 *
 * Some instant messaging protocols allow discovery of the capabilities of
 * a user's client. In Telepathy, this is represented by the Capabilities
 * interface, which lets applications advertise extra capabilities for the
 * local user, and query the interfaces supported by their contacts.
 *
 * This section documents the auto-generated C wrappers for the
 * Capabilities interface, used with #TpConnection objects.
 */

/**
 * SECTION:connection-presence
 * @title: Connection Presence interface
 * @short_description: client-side wrappers for the Presence interface
 * @see_also: #TpConnection
 *
 * Most instant messaging protocols allow users to advertise their presence
 * status. In Telepathy, this is represented by the Presence
 * interface, which lets applications advertise the presence status of the
 * local user, and query the presence status of their contacts.
 *
 * This section documents the auto-generated C wrappers for the
 * Presence interface, used with #TpConnection objects.
 */
