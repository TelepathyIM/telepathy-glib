#!/usr/bin/env python
#
# callui.py
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
gobject.threads_init()

import pygtk
import gtk

gtk.gdk.threads_init()

import dbus
from dbus.mainloop.glib import DBusGMainLoop
DBusGMainLoop(set_as_default=True)

import sys
import time

from telepathy.interfaces import *
from telepathy.constants import *

from constants import *

class CallChannelRequest:
    def __init__ (self, bus, account_path, contact,
            preferred_handler = "", audio = True, video = False,
            calltype = HANDLE_TYPE_CONTACT):
        self.bus = bus
        self.cd = bus.get_object (CHANNEL_DISPATCHER,
            '/' + CHANNEL_DISPATCHER.replace('.', '/'))

        props = {
            CHANNEL_INTERFACE + ".ChannelType": CHANNEL_TYPE_CALL,
            CHANNEL_INTERFACE + ".TargetHandleType": calltype,
              CHANNEL_INTERFACE + ".TargetID": contact,
        }

        if audio:
            props[CHANNEL_TYPE_CALL + ".InitialAudio"] = True
        if video:
            props[CHANNEL_TYPE_CALL + ".InitialVideo"] = True

        self.request_path = req_path = self.cd.CreateChannel(account_path,
            props,
            0,
            preferred_handler,
            dbus_interface = CHANNEL_DISPATCHER)

        self.req = self.bus.get_object (CHANNEL_DISPATCHER, req_path)
        self.req.connect_to_signal("Failed", self.req_failed)
        self.req.connect_to_signal("Succeeded", self.req_succeeded)
        self.req.Proceed(dbus_interface = CHANNEL_REQUEST)

    def req_failed(self, error, message):
        print "FAILURE: %s (%s)"%  (error, message)

    def req_succeeded(self):
        pass

class Account:
    CALL_CLASS = {
        CHANNEL_INTERFACE + '.ChannelType': CHANNEL_TYPE_CALL,
        CHANNEL_INTERFACE + '.TargetHandleType': HANDLE_TYPE_CONTACT
    }

    def __init__(self, bus, path):
        self.bus = bus
        self.path = path
        self.obj = bus.get_object (ACCOUNT_MANAGER, path)
        self.properties = self.obj.GetAll (ACCOUNT,
            dbus_interface=dbus.PROPERTIES_IFACE)

    def get_path(self):
        return self.path

    def name(self):
        return self.properties["DisplayName"]

    def has_connection(self):
        return self.properties["Connection"] != "/"

    def get_contacts(self):
        path = self.properties["Connection"]
        if path == "/":
            return []

        conn = self.bus.get_object (path[1:].replace("/","."), path)
        yours, channel, properties = conn.EnsureChannel (
            { CHANNEL_INTERFACE + ".ChannelType": CHANNEL_TYPE_CONTACT_LIST,
              CHANNEL_INTERFACE + ".TargetHandleType": HANDLE_TYPE_LIST,
              CHANNEL_INTERFACE + ".TargetID": "subscribe"
            },
            dbus_interface = CONNECTION_INTERFACE_REQUESTS
        )

        subscribe = self.bus.get_object (conn.bus_name, channel)
        members = subscribe.Get(CHANNEL_INTERFACE_GROUP, "Members",
            dbus_interface = dbus.PROPERTIES_IFACE)

        caps = conn.GetContactCapabilities (members,
            dbus_interface = CONNECTION_INTERFACE_CONTACT_CAPABILITIES)
        members = caps.keys()

        for k, v in caps.iteritems():
            for c in v:
                if c[0][CHANNEL_TYPE] == CHANNEL_TYPE_CALL:
                    break
            else:
                members.remove (k)

        attributes = conn.GetContactAttributes (
            dbus.Array(members, signature="u"),
            dbus.Array([], signature="s"),
            True)

        return map (lambda v: v[CONNECTION + "/contact-id"],
            attributes.itervalues())

    def supports_calls(self):
        path = self.properties["Connection"]
        if path == "/":
            return False

        conn = self.bus.get_object (path[1:].replace("/","."), path)
        classes = conn.Get (CONNECTION_INTERFACE_REQUESTS,
            'RequestableChannelClasses', dbus_interface=dbus.PROPERTIES_IFACE)

        return len ([c for c in classes if c[0] == self.CALL_CLASS]) > 0

class UI(gtk.Window):
    WIDTH=240
    HEIGHT=-1
    def __init__ (self, bus):
        gtk.Window.__init__(self)
        self.connect('destroy', lambda x: gtk.main_quit())
        self.set_resizable(False)
        self.set_size_request(self.WIDTH, self.HEIGHT)

        vbox = gtk.VBox(False, 3)
        self.add(vbox)

        # call type combo box
        self.type_store = gtk.ListStore (
            gobject.TYPE_STRING,
            gobject.TYPE_UINT)

        self.type_store.append (("1-to-1", CONNECTION_HANDLE_TYPE_CONTACT))
        self.type_store.append (("Conference",
            CONNECTION_HANDLE_TYPE_ROOM))

        self.type_combo = combobox = gtk.ComboBox (self.type_store)
        vbox.pack_start(combobox, False)

        renderer = gtk.CellRendererText()
        combobox.pack_start(renderer, True)
        combobox.set_attributes(renderer, text=0)
        combobox.set_active (0)

        # account combo box
        self.store = gtk.ListStore (gobject.TYPE_STRING,
                gobject.TYPE_BOOLEAN,
                gobject.TYPE_PYOBJECT)
        self.store.set_sort_func(0,
            (lambda m, i0, i1:
             { True: -1, False: 1}[m.get(i0, 0) < m.get(i1, 0)] ))
        self.store.set_sort_column_id(0, gtk.SORT_ASCENDING)

        f = self.store.filter_new()
        f.set_visible_func(self.filter_visible)
        self.account_combo = combobox = gtk.ComboBox(f)
        vbox.pack_start(combobox, False)

        renderer = gtk.CellRendererText()
        combobox.pack_start(renderer, True)
        combobox.set_attributes(renderer, text=0)
        combobox.connect('changed', self.account_selected)

        # contact entry box
        self.contact_store = gtk.ListStore(gobject.TYPE_STRING)

        completion = gtk.EntryCompletion ()
        completion.set_model(self.contact_store)
        completion.set_text_column(0)

        self.contact_store.set_sort_func(0, self.contact_sort)
        self.contact_store.set_sort_column_id(0, gtk.SORT_ASCENDING)

        self.contact_combo = combobox = gtk.ComboBoxEntry(self.contact_store)
        combobox.get_child().set_completion(completion)

        vbox.pack_start(combobox, False)

        bbox = gtk.HButtonBox()
        bbox.set_layout(gtk.BUTTONBOX_END)
        vbox.pack_start(bbox, True, False, 3)

        call = gtk.Button("Audio call")
        call.connect("clicked", self.start_call)
        bbox.add(call)

        call = gtk.Button("Video call")
        call.connect("clicked",
            lambda button: self.start_call(button, video=True))
        bbox.add(call)

        self.show_all()

        self.bus = bus
        self.account_mgr = bus.get_object (ACCOUNT_MANAGER,
            '/' + ACCOUNT_MANAGER.replace('.', '/'))
        self.get_accounts()

    def start_call(self, button, audio=True, video=False):
        i = self.type_combo.get_active_iter()
        (calltype, ) = self.type_combo.get_model().get(i, 1)

        i = self.account_combo.get_active_iter()
        (account, ) = self.account_combo.get_model().get(i, 2)

        contact = self.contact_combo.get_active_text().strip()

        print "* starting %s call" % ('video' if video else 'audio')
        CallChannelRequest (self.bus, account.path, contact,
            audio=audio, video=video, calltype=calltype)

    def contact_sort (self, model, i0, i1):
        if model.get(i0, 0)[0] < model.get(i1, 0)[0]:
            return -1
        else:
            return 0

    def filter_visible(self, model, titer):
        return model.get(titer, 1)[0]

    def account_selected (self, combobox):
        iter = combobox.get_active_iter()
        if iter == None:
            return None

        (account,) = combobox.get_model().get(iter, 2)

        self.contact_store.clear()

        map(lambda x: self.contact_store.insert (0, (x,)),
            account.get_contacts())

    def bail (self, *args):
        print "BAILING"
        print args
        gtk.main_quit()

    def got_accounts(self, accounts):
        for x in accounts:
            a = Account(self.bus, x)
            if a.supports_calls():
                self.store.insert(0, (a.name(), a.has_connection(), a))
            self.account_combo.set_active(0)

    def get_accounts (self):
       self.account_mgr.Get(ACCOUNT_MANAGER, "ValidAccounts",
            dbus_interface = dbus.PROPERTIES_IFACE,
            reply_handler = self.got_accounts,
            error_handler = self.bail)

if __name__ == '__main__':
    bus = dbus.SessionBus()

    UI(bus)
    gtk.main()
