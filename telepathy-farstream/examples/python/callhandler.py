# callhandler.py
# Copyright (C) 2008-2010 Collabora Ltd.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import gobject
# Need gio so GAsyncInitialbe is known
import gio

import dbus
from dbus.mainloop.glib import DBusGMainLoop
DBusGMainLoop(set_as_default=True)

from constants import *
from telepathy.interfaces import CHANNEL_INTERFACE, CLIENT, CLIENT_HANDLER
from telepathy.constants import CONNECTION_HANDLE_TYPE_CONTACT, CONNECTION_HANDLE_TYPE_ROOM
import telepathy

from callchannel import CallChannel

class CallHandler(dbus.service.Object, telepathy.server.DBusProperties):
    def __init__(self, bus, bus_name = None):
        self.bus = bus
        if bus_name == None:
            self.bus_name = "org.freedesktop.Telepathy.Client.CallDemo" \
                + bus.get_unique_name().replace(":", "_").replace(".","_")
        else:
            self.bus_name = bus_name
        self.path = "/" + self.bus_name.replace(".", "/")
        self._interfaces = set([CLIENT, CLIENT_HANDLER])
        self._prop_getters = {}
        self._prop_setters = {}

        dbus.service.Object.__init__(self, bus, self.path)
        telepathy.server.DBusProperties.__init__(self)

        self._name = dbus.service.BusName (self.bus_name, bus)

        self._implement_property_get (CLIENT,
            { "Interfaces": self._get_interfaces } )
        self._implement_property_get (CLIENT_HANDLER,
            { "HandlerChannelFilter": self._get_filters } )
        self._implement_property_get (CLIENT_HANDLER,
              { "Capabilities": self._get_capabilities } )

    def _get_interfaces(self):
        return dbus.Array(self._interfaces, signature='s')

    def _get_filters(self):
        return dbus.Array ([
            { CHANNEL_INTERFACE + ".ChannelType": CHANNEL_TYPE_CALL,
              CHANNEL_INTERFACE + ".TargetHandleType":
                CONNECTION_HANDLE_TYPE_CONTACT,
              CALL_INITIAL_AUDIO: True,
            },
            { CHANNEL_INTERFACE + ".ChannelType": CHANNEL_TYPE_CALL,
              CHANNEL_INTERFACE + ".TargetHandleType":
                CONNECTION_HANDLE_TYPE_CONTACT,
              CALL_INITIAL_VIDEO: True,
            },
            { CHANNEL_INTERFACE + ".ChannelType": CHANNEL_TYPE_CALL,
              CHANNEL_INTERFACE + ".TargetHandleType":
                CONNECTION_HANDLE_TYPE_ROOM,
              CALL_INITIAL_AUDIO: True,
            },
            { CHANNEL_INTERFACE + ".ChannelType": CHANNEL_TYPE_CALL,
              CHANNEL_INTERFACE + ".TargetHandleType":
                CONNECTION_HANDLE_TYPE_ROOM,
              CALL_INITIAL_VIDEO: True,
            }
            ],
            signature='a{sv}')

    def _get_capabilities(self):
        return dbus.Array ([
                CHANNEL_TYPE_CALL + '/gtalk-p2p',
                CHANNEL_TYPE_CALL + '/ice-udp',
                CHANNEL_TYPE_CALL +  '/video/h264',
            ], signature='s')

    def do_handle_call_channel (self, requests, bus, conn, channel, properties):
        cchannel = CallChannel(self.bus, conn, channel, properties)
        cchannel.accept()

    @dbus.service.method(dbus_interface=CLIENT_HANDLER,
                         in_signature='ooa(oa{sv})aota{sv}',
                         async_callbacks= ('_success', '_error'))
    def HandleChannels(self, account, connection, channels,
            requests, time, info, _success, _error):

        conn = telepathy.client.Connection (connection[1:].replace('/','.'),
            connection)
        # Assume there can be only one
        (channel, properties) = channels[0]

        _success()
        self.do_handle_call_channel (requests,
            self.bus, conn, channel, properties);

if __name__ == '__main__':
    gobject.threads_init()
    loop = gobject.MainLoop()
    CallHandler(dbus.SessionBus())
    loop.run()
