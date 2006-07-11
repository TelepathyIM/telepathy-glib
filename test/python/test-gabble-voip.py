#!/usr/bin/env python

# test-gabble-voip.py - test the functionality of gabble hooked up to the stream engine
#
# Copyright (C) 2005 Collabora Limited
# Copyright (C) 2005 Nokia Corporation
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import dbus
import dbus.glib
import dbus.service

assert(getattr(dbus, 'version', (0,0,0)) >= (0,51,0))

import gobject

import telepathy.server
from telepathy import *

TEST_APP_NAME = "org.freedesktop.Telepathy.TestGabbleVoip"

def connection_handle_new_channel(channel_path, channel_type, handle_type, handle, suppress_handler, **kwargs):
    print "connection_handle_new_channel"
    print "  channel_path = '%s'" % (channel_path,)
    print "  channel_type = '%s'" % (channel_type,)
    print "  handle_type = %d" % (handle_type,)
    print "  handle = %d" % (handle,)
    print "  suppress_handler = %s" % (suppress_handler,)
    print "  kwargs:", kwargs
    print

    if not channel_type == CHANNEL_TYPE_STREAMED_MEDIA:
        return

    print "connection_handle_new_channel: detected a CHANNEL_TYPE_STREAMED_MEDIA channel!"

    chandler.HandleChannel(kwargs["connection_sender"],
                           kwargs["connection_path"],
                           channel_type,
                           channel_path,
                           handle_type,
                           handle)


if __name__ == '__main__':
    global bus
    global stream_obj
    global chandler
    
    bus = dbus.Bus()
    bus_name = dbus.service.BusName(TEST_APP_NAME, bus=bus)

    stream_obj = bus.get_object("org.freedesktop.Telepathy.VoipEngine",
                               "/org/freedesktop/Telepathy/VoipEngine")
    chandler = dbus.Interface(stream_obj, "org.freedesktop.Telepathy.ChannelHandler")

    bus.add_signal_receiver(connection_handle_new_channel,
                            "NewChannel",
                            CONN_INTERFACE,
                            sender_keyword="connection_sender",
                            path_keyword="connection_path")
    
    gobject.MainLoop().run()

