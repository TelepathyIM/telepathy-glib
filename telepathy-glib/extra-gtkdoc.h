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
 * @title: Service-side Channel base interface
 * @short_description: GInterface for Telepathy Channel objects
 * @see_also: #TpChannelIface
 *
 * This interface (auto-generated from the Telepathy spec) makes it easier
 * to export objects implementing the Telepathy Channel.
 */

/**
 * SECTION:svc-debug
 * @title: Service-side Debug base interface
 * @short_description: GInterface for Telepathy Debug objects
 *
 * This interface (auto-generated from the Telepathy spec) makes it easier
 * to export objects implementing the Telepathy Debug interface.
 *
 * #TpDebugSender provides a reference implementation of the Debug object.
 *
 * Since: 0.7.36
 */

/**
 * SECTION:svc-channel-group
 * @title: Service-side Channel Group and Conference interfaces
 * @short_description: Groups of contacts
 * @see_also: #TpGroupMixin
 *
 * Many Telepathy Channel objects can be seen as representing groups or
 * sets of contacts. The Telepathy specification represents this by a common
 * interface, Group. This section documents the auto-generated GInterface
 * used to implement the Group interface.
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
 *
 * Since 0.11.16, telepathy-glib also includes basic support for the
 * Conference interface, which represents a Group channel that can be
 * initiated by merging or upgrading individual 1-1 channels.
 */

/**
 * SECTION:svc-channel-room
 * @title: Service-side room interfaces
 * @short_description: room-related functionality for channels
 *
 * This collection of interfaces is used to expose various aspects of the
 * configuration of chat rooms.
 *
 * #TpSvcChannelInterfaceRoom consists of a pair of requestable,
 * immutable properties: <code>"RoomName"</code> and
 * <code>"Server"</code>; and a pair of immutable properties:
 * <code>"Creator"</code>, <code>"CreatorHandle"</code>, and
 * <code>"CreationTimestamp"</code>. It has no methods or signals. It
 * should be implemented on channels representing a chat room (whether
 * it be a text chat, a multi-user call, or some other media type).
 *
 * #TpSvcChannelInterfaceSubject may be implemented by channels which have a
 * subject (or topic, depending on your protocol's terminology of choice). This
 * will usually be in addition to #TpSvcChannelInterfaceRoom, though in theory
 * a 1-1 channel could have a subject. In addition to its single method, it
 * defines a set of read-only properties, namely <code>"Subject"</code>,
 * <code>"Actor"</code>, <code>"ActorHandle"</code>, <code>"Timestamp"</code>,
 * and <code>"CanSet"</code>. Changes should be signalled using
 * tp_dbus_properties_mixin_emit_properties_changed().
 *
 * #TpSvcChannelInterfaceRoomConfig provides a vast array of properties for
 * other aspects of a chat room's configuration (such as the maximum number of
 * participants, and whether the room is password-protected). Channels with
 * this interface will typically implement the other two, too.
 *
 * Since: 0.15.8
 */

/**
 * SECTION:svc-channel-text
 * @title: Text channels
 * @short_description: service-side interfaces for the Text channel type, and
 *  the Chat State, Password and SMS interfaces
 * @see_also: #TpTextMixin
 *
 * A major use for instant messaging is obviously to send messages.
 * Channels of type Text represent conversations or chat rooms using short
 * real-time messages, including SMS.
 *
 * This section documents the auto-generated GInterfaces used to implement
 * the Text channel type, and some interfaces used in conjunction with it.
 */

/**
 * SECTION:svc-channel-file-transfer
 * @title: File Transfer channels
 * @short_description: service-side interface for the File Transfer channel type
 *
 * This section documents the auto-generated GInterface used to implement
 * the File Transfer channel type.
 */

/**
 * SECTION:svc-channel-media
 * @title: Media channels
 * @short_description: service-side interfaces for the Streamed Media channel
 *  type, and the Call State, DTMF and Media Signalling interfaces
 *
 * This section documents the auto-generated C wrappers for the Streamed Media
 * channel type, and some interfaces which are optionally supported by
 * channels of this type.
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
 * terms of Jingle or SDP, for instance).
 *
 * Channels of type Streamed Media may also support the DTMF and
 * CallState interfaces.
 */

/**
 * SECTION:svc-channel-tubes
 * @title: Tubes channels
 * @short_description: service-side interface for the Tubes channel type
 *
 * A "tube" is a mechanism for arbitrary data transfer.
 * This section documents the auto-generated C wrappers for the Tubes
 * channel type.
 */

/**
 * SECTION:svc-channel-tube
 * @title: Tube channels
 * @short_description: service-side interface for the Tube channel interface,
 *  StreamTube channel type and DBusTube channel type.
 *
 * A "tube" is a mechanism for arbitrary data transfer.
 * This section documents the auto-generated C wrappers for the Tube
 * channel interface, StreamTube channel type and DBusTube channel type.
 */

/**
 * SECTION:svc-channel-contactlist
 * @title: Contact List channels
 * @short_description: service-side interface for the Contact List channel type
 *
 * Many instant messaging protocols have the a concept of a contact list,
 * roster or buddy list. Some protocols also have user-defined groups or tags
 * which can be represented as subsets of the roster.
 *
 * This section documents the auto-generated C wrappers for the Contact List
 * channel type.
 */

/**
 * SECTION:svc-channel-roomlist
 * @title: Room List channels
 * @short_description: service-side interface for the Room List channel type
 *
 * Many instant messaging protocols allow named chatrooms to be listed.
 * This section documents the auto-generated C wrappers for the Room List
 * channel type.
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
 * SECTION:svc-account
 * @title: Service-side Account interfaces
 * @short_description: GInterfaces for Telepathy Account objects
 *
 * These interfaces (auto-generated from the Telepathy spec) make it easier
 * to export objects implementing the Telepathy Account and its
 * optional interfaces, with the correct method and signal signatures,
 * and emit signals from those objects in a type-safe way.
 *
 * You don't need these interfaces unless you're implementing a
 * Telepathy AccountManager, such as Mission Control.
 */

/**
 * SECTION:svc-account-manager
 * @title: Service-side Account Manager interface
 * @short_description: GInterface for Telepathy AccountManager objects
 *
 * The #TpSvcAccountManager interface (auto-generated from the Telepathy
 * spec) makes it easier to export an object implementing the Telepathy
 * AccountManager interface, with the correct method and signal signatures,
 * and emit signals from that object in a type-safe way.
 *
 * You don't need these interfaces unless you're implementing a
 * Telepathy AccountManager, such as Mission Control.
 */

/**
 * SECTION:svc-channel-dispatcher
 * @title: Service-side Channel Dispatcher interface
 * @short_description: GInterfaces for Telepathy ChannelDispatcher object
 *
 * The #TpSvcChannelDispatcher interface (auto-generated from the Telepathy
 * spec) makes it easier to export an object implementing the Telepathy
 * ChannelDispatcher interface, with the correct method and signal signatures,
 * and emit signals from that object in a type-safe way.
 *
 * Similarly, #TpSvcChannelDispatcherInterfaceOperationList helps to
 * implement the optional OperationList interface.
 *
 * You don't need these interfaces unless you're implementing a
 * Telepathy ChannelDispatcher, such as Mission Control.
 */

/**
 * SECTION:svc-channel-dispatch-operation
 * @title: Service-side Channel Dispatch Operation interface
 * @short_description: GInterface for Telepathy ChannelDispatchOperation object
 *
 * This interface (auto-generated from the Telepathy
 * spec) makes it easier to export an object implementing the Telepathy
 * ChannelDispatchOperation interface, with the correct method and signal
 * signatures, and emit signals from that object in a type-safe way.
 *
 * You don't need these interfaces unless you're implementing a
 * Telepathy ChannelDispatcher, such as Mission Control.
 */

/**
 * SECTION:svc-channel-request
 * @title: Service-side Channel Request interface
 * @short_description: GInterface for Telepathy ChannelRequest object
 *
 * This interface (auto-generated from the Telepathy
 * spec) makes it easier to export an object implementing the Telepathy
 * ChannelRequest interface, with the correct method and signal
 * signatures, and emit signals from that object in a type-safe way.
 *
 * You don't need these interfaces unless you're implementing a
 * Telepathy ChannelDispatcher, such as Mission Control.
 */

/**
 * SECTION:svc-channel-dispatcher
 * @title: Service-side Channel Dispatcher interface
 * @short_description: GInterfaces for Telepathy ChannelDispatcher object
 *
 * The #TpSvcChannelDispatcher interface (auto-generated from the Telepathy
 * spec) makes it easier to export an object implementing the Telepathy
 * ChannelDispatcher interface, with the correct method and signal signatures,
 * and emit signals from that object in a type-safe way.
 *
 * Similarly, #TpSvcChannelDispatcherInterfaceOperationList helps to
 * implement the optional OperationList interface.
 *
 * You don't need these interfaces unless you're implementing a
 * Telepathy ChannelDispatcher, such as Mission Control.
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
 * SECTION:svc-client
 * @title: Service-side Client interfaces
 * @short_description: interfaces used to be an Observer, Approver and Handler
 *
 * These interfaces (auto-generated from the telepathy spec) make it easier
 * to export the objects used to implement a Telepathy client.
 *
 * Clients such as loggers, new message notification windows and chat UIs
 * should implement some or all of the Client types (Observer, Approver and/or
 * Handler): see telepathy-spec for details.
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
 * Changed in 0.7.0: in older versions, some of these constants were in
 * base-connection.h and base-connection-manager.h.
 *
 * Since: 0.7.0
 */

/**
 * SECTION:version
 * @title: Version information
 * @short_description: Checking the telepathy-glib version
 *
 * Since 0.19.0, telepathy-glib provides version information similar
 * to that used in GLib.
 *
 * Typical usage from configure.ac is similar to GLib's:
 *
 * |[
 * AC_DEFINE([TP_VERSION_MIN_REQUIRED], [TP_VERSION_0_18],
 *   [Ignore deprecations newer than this branch])
 * AC_DEFINE([TP_VERSION_MAX_ALLOWED], [TP_VERSION_0_20],
 *   [Prevent use of APIs newer than this branch])
 * ]|
 *
 * or you can define the macros directly on the compiler command line
 * if required:
 *
 * |[
 * cc -o foo foo.c \
 *     -DTP_VERSION_MIN_REQUIRED=TP_VERSION_0_18 \
 *     -DTP_VERSION_MAX_ALLOWED=TP_VERSION_0_20 \
 *     ${TP_CFLAGS} ${TP_LIBS}
 * ]|
 *
 * This functionality was added in telepathy-glib 0.19.0, but it
 * is safe to define the TP_VERSION_MIN_REQUIRED and TP_VERSION_MAX_ALLOWED
 * macros even for older versions of telepathy-glib, as long as you do
 * not try to expand them.
 */

/**
 * TP_MAJOR_VERSION:
 *
 * The major version of telepathy-glib (e.g. the 0 in 0.18.1) at the time your
 * code was compiled.
 */

/**
 * TP_MINOR_VERSION:
 *
 * The minor version of telepathy-glib (e.g. the 18 in 0.18.1) at the time your
 * code was compiled.
 *
 * Odd minor versions indicate a development branch; even minor versions
 * indicate a stable branch.
 */

/**
 * TP_MICRO_VERSION:
 *
 * The micro version of telepathy-glib (e.g. the 1 in 0.18.1) at the time your
 * code was compiled.
 *
 * Within a stable branch (even minor version), micro versions fix bugs
 * but do not add features.
 *
 * Within a development branch (odd minor version), micro versions can
 * fix bugs and/or add features.
 */

/**
 * TP_VERSION_0_16: (skip)
 *
 * A constant representing the telepathy-glib 0.16 stable branch,
 * and the 0.15 development branch that led to it.
 */

/**
 * TP_VERSION_0_18: (skip)
 *
 * A constant representing the telepathy-glib 0.18 stable branch,
 * and the 0.17 development branch that led to it.
 */

/**
 * TP_VERSION_0_20: (skip)
 *
 * A constant representing the telepathy-glib 0.20 stable branch,
 * and the 0.19 development branch that led to it.
 */

/**
 * TP_VERSION_1_0: (skip)
 *
 * A constant representing the telepathy-glib 1.0 stable branch,
 * and the 0.99 development branch that led to it.
 */

/**
 * TP_VERSION_MIN_REQUIRED: (skip)
 *
 * A version-number constant like %TP_VERSION_0_18.
 *
 * This may be defined to a value like %TP_VERSION_0_18 by users of
 * telepathy-glib, to set the minimum version they wish to
 * require. Warnings will be issued for functions deprecated in or
 * before that version.
 *
 * If not defined, the default value is the previous stable branch.
 */

/**
 * TP_VERSION_MAX_ALLOWED: (skip)
 *
 * A version-number constant like %TP_VERSION_0_18.
 *
 * This may be defined to a value like %TP_VERSION_0_18 by users of
 * telepathy-glib, to set the maximum version they wish to
 * depend on. Warnings will be issued for functions deprecated in or
 * before that version.
 *
 * If not defined, the default value in stable branches is that stable
 * branch, and the default value in development branches is the next
 * stable branch.
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
 * TP_NUM_CONNECTION_STATUSES. The pluralization is currently hard-coded
 * in the conversion scripts, but should move into the specification
 * in future.
 *
 * Constants LAST_TP_CONNECTION_STATUS, etc. are also provided. These are
 * deprecated and will be removed in a future release.
 */

/**
 * SECTION:interfaces
 * @title: Telepathy protocol interface and property names
 * @short_description: D-Bus interface and property names from the
 *  Telepathy spec
 *
 * This header exposes the interface names from the Telepathy specification
 * as cpp defines for strings, such as %TP_IFACE_PROPERTIES_INTERFACE.
 * It is automatically generated from the specification.
 *
 * Since 0.7.0 it also provides cpp defines like
 * %TP_IFACE_QUARK_PROPERTIES_INTERFACE, which expand to function calls that
 * return GQuarks for the same strings.
 *
 * Since 0.9.2 it also provides cpp defines like
 * %TP_PROP_CHANNEL_INTERFACE_GROUP_GROUP_FLAGS, which expand to string
 * constants representing fully-qualified D-Bus property names, like
 * <literal>org.freedesktop.Telepathy.Channel.Interface.Group.GroupFlags</literal>.
 *
 * Since 0.11.5 it also provides cpp defines like
 * %TP_TOKEN_CONNECTION_INTERFACE_ALIASING_ALIAS for contact attributes like
 * "org.freedesktop.Telepathy.Connection.Interface.Aliasing/alias",
 * and defines like
 * %TP_TOKEN_CHANNEL_INTERFACE_MEDIA_SIGNALLING_ICE_UDP
 * for handler capability tokens like
 * "org.freedesktop.Telepathy.Channel.Interface.MediaSignalling/gtalk-p2p".
 * (These were present in an incorrect form since 0.11.3.)
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
 * @title: Group and Conference interfaces on Channels
 * @short_description: client-side wrappers for Group and Conference
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
 *
 * Some Group channels also have the Conference interface, representing a
 * group which can be initiated by upgrading or merging one or more 1-1
 * channels.
 */

/**
 * SECTION:channel-room
 * @title: Room-related interfaces on Channels
 * @short_description: client-side wrappers for Room, RoomConfig and Subject
 *
 * This family of interfaces exposes aspects of chat rooms' configuration, and
 * provides API to modify it (where permitted). Most of the API is in terms of
 * D-Bus properties; they may be retrieved using
 * tp_cli_dbus_properties_call_get_all(), and changes monitored using
 * tp_cli_dbus_properties_connect_to_properties_changed().
 *
 * #TP_IFACE_CHANNEL_INTERFACE_ROOM consists only of a pair of requestable,
 * immutable properties: #TP_PROP_CHANNEL_INTERFACE_ROOM_ROOM_NAME and
 * #TP_PROP_CHANNEL_INTERFACE_ROOM_SERVER.
 *
 * In addition to #TP_IFACE_CHANNEL_INTERFACE_SUBJECT's single method, it
 * defines a set of read-only properties: <code>"Subject"</code>,
 * <code>"Actor"</code>, <code>"ActorHandle"</code>, <code>"Timestamp"</code>,
 * and <code>"CanSet"</code>.
 *
 * #TP_IFACE_CHANNEL_INTERFACE_ROOM_CONFIG provides a vast array of properties
 * for other aspects of a chat room's configuration (such as the maximum number
 * of participants, and whether the room is password-protected).
 *
 * Since: 0.15.8
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
 * SECTION:channel-file-transfer
 * @title: File transfer
 * @short_description: client-side wrappers for the File Transfer channel type
 * @see_also: #TpChannel
 *
 * This section documents the auto-generated C wrappers for the File Transfer
 * channel type.
 */

/**
 * SECTION:channel-media
 * @title: Media channels
 * @short_description: client-side wrappers for the Streamed Media channel
 *  type, and the Call State, DTMF and Media Signalling interfaces
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
 * Channels of type Streamed Media may also support the DTMF and
 * CallState interfaces.
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
 * SECTION:channel-tube
 * @title: Tube channels
 * @short_description: client-side wrappers for the Tube channel interface,
 *  StreamTube channel type and DBusTube channel type.
 * @see_also: channel-group
 *
 * A "tube" is a mechanism for arbitrary data transfer.
 * This section documents the auto-generated C wrappers for the Tube
 * channel interface, StreamTube channel type and DBusTube channel type.
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
 * @title: Connection ContactCapabilities and Capabilities interfaces
 * @short_description: client-side wrappers for the capabilities interfaces
 * @see_also: #TpConnection
 *
 * Some instant messaging protocols allow discovery of the capabilities of
 * a user's client. In Telepathy, this is represented by the
 * ContactCapabilities interface, which lets applications advertise extra
 * capabilities for the local user, and query the interfaces supported by
 * their contacts.
 *
 * The Capabilities interface is an older API with similar functionality.
 *
 * This section documents the auto-generated C wrappers for the
 * ContactCapabilities and Capabilities interfaces, used with
 * #TpConnection objects.
 */

/**
 * SECTION:connection-contacts
 * @title: Connection Contacts interface
 * @short_description: client-side wrappers for the Contacts interface
 * @see_also: #TpConnection
 *
 * This interface allows a client to get information from various connection
 * interfaces in one dbus call.
 *
 * This section documents the auto-generated C wrappers for the
 * Contacts interface, used with #TpConnection objects.
 */

/**
 * SECTION:connection-contact-list
 * @title: Connection ContactList, ContactGroups and ContactBlocking interfaces
 * @short_description: client-side wrappers for the ContactList,
 *  ContactGroups and ContactBlocking interfaces
 * @see_also: #TpConnection
 *
 * This interface allows a client to obtain a server-stored contact list
 * and contacts' groups.
 *
 * This section documents the auto-generated C wrappers for the
 * ContactList, ContactGroups and ContactBlocking interfaces, used
 * with #TpConnection objects.
 */

/**
 * SECTION:connection-requests
 * @title: Connection Requests interface
 * @short_description: client-side wrappers for the Requests interface
 * @see_also: #TpConnection
 *
 * This interface allows a client to request new channels from a connection,
 * and to listen to signals indicating that channels have been created and
 * closed.
 *
 * This section documents the auto-generated C wrappers for the Requests
 * interface, used with #TpConnection objects.
 */

/**
 * SECTION:connection-simple-presence
 * @title: Connection SimplePresence interface
 * @short_description: client-side wrappers for the SimplePresence interface
 * @see_also: #TpConnection
 *
 * Most instant messaging protocols allow users to advertise their presence
 * status. In Telepathy, this is represented by the SimplePresence
 * interface, which lets applications advertise the presence status of the
 * local user, and query the presence status of their contacts.
 *
 * This section documents the auto-generated C wrappers for the
 * SimplePresence interface, used with #TpConnection objects.
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

/**
 * SECTION:connection-location
 * @title: Connection Location interface
 * @short_description: client-side wrappers for the Location interface
 * @see_also: #TpConnection
 *
 * Some instant messaging protocols support "rich presence" functionality,
 * such as geolocation (advertising the user's location to authorized
 * contacts, and receiving contacts' locations).
 *
 * This section documents the auto-generated C wrappers for the
 * Location interface, used with #TpConnection objects.
 */

/**
 * SECTION:connection-balance
 * @title: Connection Balance interface
 * @short_description: client-side wrappers for the Balance interface
 * @see_also: #TpConnection
 *
 * In some real-time communication services the user can pay for certain
 * services, typically calls to the PSTN, in advance. In (at least) Skype,
 * it's possible to query the current balance in a machine-readable way.
 *
 * This section documents the auto-generated C wrappers for the
 * Balance interface, used with #TpConnection objects.
 *
 * Since: 0.11.0
 */

/**
 * SECTION:connection-contact-info
 * @title: Connection ContactInfo interface
 * @short_description: client-side wrappers for the ContactInfo interface
 * @see_also: #TpConnection
 *
 * An interface for requesting information about a contact on a given
 * connection. The information is represented as a list of fields forming a
 * structured representation of a vCard (as defined by RFC 2426), using field
 * names and semantics defined therein.
 *
 * This section documents the auto-generated C wrappers for the
 * ContactInfo interface, used with #TpConnection objects.
 *
 * Since: 0.11.3
 */

/**
 * SECTION:cli-anonymity
 * @title: Connection and Channel Anonymity interfaces
 * @short_description: client-side wrappers for the Anonymity interfaces
 * @see_also: #TpConnection, #TpChannel
 *
 * In some protocols, mainly those that interact with the PSTN, it's possible
 * to make a call without disclosing the originating identity (e.g. phone
 * number). The Anonymity interfaces on the Connection and Channel can be used
 * to control this feature in Telepathy.
 *
 * This section documents the auto-generated C wrappers for the
 * Anonymity interfaces, used with #TpConnection and #TpChannel objects.
 *
 * Since: 0.11.7
 */

/**
 * SECTION:svc-anonymity
 * @title: Service-side Connection and Channel Anonymity interfaces
 * @short_description: GInterfaces to implement the Anonymity interfaces
 *
 * In some protocols, mainly those that interact with the PSTN, it's possible
 * to make a call without disclosing the originating identity (e.g. phone
 * number). The Anonymity interfaces on the Connection and Channel can be used
 * to control this feature in Telepathy.
 *
 * This section documents the auto-generated GInterfaces used to implement the
 * Anonymity interfaces.
 *
 * Since: 0.11.7
 */

/**
 * SECTION:cli-service-point
 * @title: Connection and Channel ServicePoint interfaces
 * @short_description: client-side wrappers for the ServicePoint interfaces
 * @see_also: #TpConnection, #TpChannel
 *
 * In some protocols, mainly those that interact with the PSTN, it's possible
 * to contact the emergency services or other public service answering points.
 * The ServicePoint interfaces on the Connection and Channel can be used
 * to discover how to contact these service points, and detect whether a call
 * in progress is communicating with a service point.
 *
 * This section documents the auto-generated C wrappers for the
 * ServicePoint interfaces, used with #TpConnection and #TpChannel objects.
 *
 * Since: 0.11.7
 */

/**
 * SECTION:svc-service-point
 * @title: Service-side Connection and Channel ServicePoint interfaces
 * @short_description: GInterfaces to implement the ServicePoint interfaces
 *
 * In some protocols, mainly those that interact with the PSTN, it's possible
 * to contact the emergency services or other public service answering points.
 * The ServicePoint interfaces on the Connection and Channel can be used
 * to discover how to contact these service points, and detect whether a call
 * in progress is communicating with a service point.
 *
 * This section documents the auto-generated GInterfaces used to implement the
 * ServicePoint interfaces.
 *
 * Since: 0.11.7
 */

/**
 * SECTION:connection-cellular
 * @title: Connection Cellular interface
 * @short_description: client-side wrappers for the Cellular interface
 * @see_also: #TpConnection
 *
 * An interface for connections to cellular telephony (GSM, CDMA etc.), which
 * provides properties and signals that aren't applicable to other protocols.
 *
 * Since: 0.11.9
 */

/**
 * SECTION:svc-protocol
 * @title: Service-side Protocol interface
 * @short_description: GInterface for Telepathy Protocol objects
 * @see_also: #TpBaseProtocol
 *
 * The #TpSvcProtocol interface (auto-generated from the Telepathy
 * spec) makes it easier to export an object implementing the Telepathy
 * Protocol interface.
 *
 * Since: 0.11.11
 */

/**
 * SECTION:channel-contactsearch
 * @title: Contact Search channels
 * @short_description: client-side wrappers for the Contact Search channel type
 * @see_also: #TpChannel
 *
 * Some instant messaging protocols allow searching for contacts by name or
 * other details. In Telepathy, each search attempt is represented as a
 * Channel.
 *
 * This section documents the auto-generated C wrappers for the Contact Search
 * channel type.
 *
 * Since: 0.11.11
 */

/**
 * SECTION:svc-channel-contactsearch
 * @title: Contact Search channels
 * @short_description: service-side interface for the Contact Search channel
 *  type
 *
 * Some instant messaging protocols allow searching for contacts by name or
 * other details. In Telepathy, each search attempt is represented as a
 * Channel.
 *
 * This section documents the auto-generated C wrappers for the Contact Search
 * channel type.
 *
 * Since: 0.11.11
 */

/**
 * SECTION:svc-tls
 * @title: Service-side TLS interfaces
 * @short_description: GInterfaces to implement Chan.T.ServerTLSConnection
 *
 * Channel.Type.ServerTLSConnection can be handled by clients to check
 * servers' TLS certificates interactively. The actual certificates are
 * represented by a separate TLSCertificate object.
 *
 * Since: 0.11.16
 */

/**
 * SECTION:connection-client-types
 * @title: Connection ClientTypes interface
 * @short_description: client-side wrappers for the ClientTypes interface
 *
 * On some protocols it's possible to determine the type of client another
 * user is using, ranging from a simple "phone or not?" indicator to a
 * classification into several types of user interface. Telepathy represents
 * these using the client types defined by XMPP.
 *
 * This section documents the auto-generated C wrappers for the
 * ClientTypes interface, used with #TpConnection objects.
 *
 * Since: 0.13.1
 */

/**
 * SECTION:connection-mail
 * @title: Connection MailNotification interface
 * @short_description: client-side wrappers for the MailNotification interface
 *
 * Some service providers offer both real-time communications and e-mail, and
 * integrate them by providing "new mail" notifications over the real-time
 * communication protocol.
 *
 * This section documents the auto-generated C wrappers for the
 * MailNotification interface, used with #TpConnection objects.
 *
 * Since: 0.13.1
 */

/**
 * SECTION:connection-powersaving
 * @title: Connection PowerSaving interface
 * @short_description: client-side wrappers for the PowerSaving interface
 *
 * Some connection manager implementations can be instructed to try to
 * save power on mobile devices by suppressing non-essential traffic, such
 * as presence notifications. This section documents auto-generated C
 * wrappers for the PowerSaving D-Bus interface.
 *
 * Since: 0.13.7
 */

/**
 * SECTION:channel-auth
 * @title: Channel Authentication interfaces
 * @short_description: client-side wrappers for authentication channels
 *
 * The ServerAuthentication channel type represents a request for client/UI
 * processes to carry out authentication with a server, including password
 * authentication (prove that you are who you say you are) and captcha
 * authentication (prove that you are not a bot).
 *
 * Since: 0.13.7
 */

/**
 * SECTION:svc-channel-auth
 * @title: Service-side Channel Authentication interfaces
 * @short_description: GInterfaces to implement authentication channels
 *
 * The ServerAuthentication channel type represents a request for client/UI
 * processes to carry out authentication with a server.
 *
 * The SASLAuthentication interface allows authentication via SASL, and also
 * allows providing a simple password to the connection manager for it to
 * use with SASL or non-SASL mechanisms.
 *
 * The CaptchaAuthentication interface (since 0.17.5) allows
 * interactive captcha-solving so that the user can prove that they are not
 * a bot, on protocols requiring this.
 *
 * Since: 0.13.7
 */

/**
 * SECTION:svc-channel-securable
 * @title: Service-side Securable interface
 * @short_description: GInterface to indicate channels' security level
 *
 * The Securable channel interface represents a channel that might be
 * end-to-end encrypted and/or protected from man-in-the-middle attacks.
 *
 * Since: 0.13.7
 */

/**
 * SECTION:svc-channel-ft-metadata
 * @title: File transfer Metadata interface
 * @short_description: GInterface to implement metadata file transfer interface
 *
 * The Metadata file transfer channel interface exists to provide a
 * mechanism to include arbitrary additional information in file
 * transfers. For example, one might want to send a document and
 * include the number of times the character P appeared in the file,
 * so would add NumberOfPs=42 to the Metadata property.
 *
 * Since: 0.17.1
 */

/**
 * SECTION:connection-addressing
 * @title: Connection Addressing interface
 * @short_description: client-side wrappers for the Addressing interface
 * @see_also: #TpConnection
 *
 * An interface for connections in protocols where contacts' unique
 * identifiers can be expressed as vCard fields and/or URIs.
 *
 * Since: 0.17.5
 */

/**
 * SECTION:svc-channel-call
 * @title: Service-side Channel Call interface
 * @short_description: GInterface to implement call channels
 *
 * Call channels represent real-time audio or video streaming, including
 * voice over IP, webcams, and telephony.
 *
 * Since: 0.17.5
 */

/**
 * SECTION:cli-call-channel
 * @title: Channel Call interfaces
 * @short_description: client-side wrappers for call channels
 *
 * Call channels represent real-time audio or video streaming, including
 * voice over IP, webcams, and telephony.
 *
 * Since: 0.17.5
 */

/**
 * SECTION:cli-call-content
 * @title: Channel Call content interfaces
 * @short_description: client-side wrappers for call contents
 *
 * Represents the contents of a call.
 *
 * Since: 0.17.5
 */

/**
 * SECTION:cli-call-stream
 * @title: Channel Call stream interfaces
 * @short_description: client-side wrappers for call streams
 *
 * Represents the streams of a call.
 *
 * Since: 0.17.5
 */

/**
 * SECTION:cli-call-misc
 * @title: Channel Call misc interfaces
 * @short_description: client-side wrappers for misc call interfaces
 *
 * Misc interfaces for calls.
 *
 * Since: 0.17.5
 */
