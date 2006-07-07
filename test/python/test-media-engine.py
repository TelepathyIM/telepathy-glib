#!/usr/bin/env python

# test-media-engine.py - test the functionaity of the media engine
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

TEST_APP_NAME = "org.freedesktop.Telepathy.TestVoipEngine"
TEST_CONNECTION_PATH = "/org/freedesktop/Telepathy/TestConnection"
TEST_CHANNEL_PATH = "/org/freedesktop/Telepathy/TestChannel"
TEST_SESSION_PATH = "/org/freedesktop/Telepathy/TestSession"
TEST_STREAM_PATH = "/org/freedesktop/Telepathy/TestStream"

SELF_HANDLE = 1
OTHER_HANDLE = 2

class DummyConnection(dbus.service.Object):
    def __init__(self, bus_name, object_path):
        dbus.service.Object.__init__(self, bus_name, object_path)
        self._name = bus_name

    def get_channel_path(self):
        return TEST_CHANNEL_PATH

    def remove_channel(self,channel):
        pass

    def GetSelfHandle(self):
        return SELF_HANDLE

class TestMediaStreamHandler(telepathy.server.MediaStreamHandler):
    def __init__(self, bus_name):
        telepathy.server.MediaStreamHandler.__init__(self, bus_name, TEST_STREAM_PATH)
 
    def Ready(self):
        print "TestMediaStreamHandler::Ready called, creating new stream"

class TestMediaSessionHandler(telepathy.server.MediaSessionHandler):
    def __init__(self, bus_name):
        telepathy.server.MediaSessionHandler.__init__(self,bus_name, TEST_SESSION_PATH)
        self._bus_name = bus_name
        
    def Ready(self):
        print "TestMediaSessionHandler::Ready called, creating new stream"
        self.NewMediaStreamHandler(TEST_STREAM_PATH, 
            MEDIA_STREAM_TYPE_AUDIO,
            MEDIA_STREAM_DIRECTION_BIDIRECTIONAL)
        print "Emitted %s signal" % MEDIA_SESSION_HANDLER

class TestStreamedMediaChannel(telepathy.server.ChannelTypeStreamedMedia, telepathy.server.ChannelInterfaceGroup):
    def __init__(self, conn, handle):
        telepathy.server.ChannelTypeStreamedMedia.__init__(self, conn, handle)
        telepathy.server.ChannelInterfaceGroup.__init__(self)

    def GetSessionHandlers(self):
        print "GetSessionHandlers called"
        return [(OTHER_HANDLE, TEST_SESSION_PATH, "rtp")]
        

if __name__ == '__main__':
    bus = dbus.Bus()
    bus_name = dbus.service.BusName(TEST_APP_NAME, bus=bus)
    manager = DummyConnection(bus_name, TEST_CONNECTION_PATH)
    channel = TestStreamedMediaChannel(manager, 0)
    session = TestMediaSessionHandler(bus_name)
    stream  = TestMediaStreamHandler(bus_name)

    proxy_obj = bus.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus')
    dbus_iface = dbus.Interface(proxy_obj, 'org.freedesktop.DBus')

    try:
        dbus_iface.StartServiceByName("org.freedesktop.Telepathy.VoipEngine",0)
    except:
        print "failed to activate org.freedesktop.Telepathy.VoipEngine, continuing anyway..."

    media_engine = bus.get_object("org.freedesktop.Telepathy.VoipEngine",
            "/org/freedesktop/Telepathy/VoipEngine")
    channel_handler = dbus.Interface(media_engine,
        "org.freedesktop.Telepathy.ChannelHandler")
    channel_handler.HandleChannel(TEST_APP_NAME, TEST_CONNECTION_PATH,
        CHANNEL_TYPE_STREAMED_MEDIA, TEST_CHANNEL_PATH, 0, 0)

    gobject.MainLoop().run()

    
    
