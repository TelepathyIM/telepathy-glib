#!/usr/bin/env python
# encoding: utf-8
#
# A simple Telepathy dialler, suitable for use when a nicer UI you're
# developing doesn't work yet.
#
# Copyright © 2012 Collabora Ltd.
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

from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Gtk
from gi.repository import TelepathyGLib as Tp
import sys

GObject.threads_init()

# not available via g-i
USER_ACTION_TIME_NOT_USER_ACTION = 0
ACCOUNT_MANAGER_FEATURE_CORE = GLib.quark_from_string(
    'tp-account-manager-feature-core')

class Dialler(Gtk.Application):

    def __init__(self):
        Gtk.Application.__init__(self,
                application_id='im.telepathy.TpGLib.Example.PythonDialler',
                flags=Gio.ApplicationFlags.NON_UNIQUE)

        self.dbus = Tp.DBusDaemon.dup()
        self.am = Tp.AccountManager.dup()

        self.accounts = Gtk.ListStore(str, object)
        self.potential_handlers = set()
        self.handlers = Gtk.ListStore(str, str)

        self.window = None

    def do_activate(self):
        if self.window is not None:
            self.window.present()
            return

        self.window = Gtk.Window()
        self.grid = Gtk.Grid()
        self.grid.props.margin = 6
        self.grid.props.column_spacing = 6
        self.grid.props.row_spacing = 6
        self.window.add(self.grid)

        row = 0

        self.grid.attach(Gtk.Label('local account:'), 0, row, 1, 1)
        self.account_combo = Gtk.ComboBox.new_with_model(self.accounts)
        renderer = Gtk.CellRendererText()
        self.account_combo.pack_start(renderer, True)
        self.account_combo.add_attribute(renderer, "text", 0)
        self.grid.attach(self.account_combo, 1, row, 1, 1)
        row += 1

        self.grid.attach(Gtk.Label('remote target:'), 0, row, 1, 1)
        self.target_entry = Gtk.Entry()
        self.target_entry.set_text('smcv@example.com')
        self.grid.attach(self.target_entry, 1, row, 1, 1)
        row += 1

        self.grid.attach(Gtk.Label('preferred handler:'), 0, row, 1, 1)
        self.handler_combo = Gtk.ComboBox.new_with_model(self.handlers)
        renderer = Gtk.CellRendererText()
        self.handler_combo.pack_start(renderer, True)
        self.handler_combo.add_attribute(renderer, "text", 0)
        self.grid.attach(self.handler_combo, 1, row, 1, 1)
        row += 1

        self.dial_button = Gtk.Button('dial')
        self.dial_button.connect('clicked', self._dial_clicked)
        self.grid.attach(self.dial_button, 0, row, 2, 1)
        row += 1

        self.dial_button.props.sensitive = False
        self.window.show_all()
        self.add_window(self.window)

        self.am.prepare_async([], self._am_cb, None)
        Gio.bus_get(Gio.BusType.SESSION, None,
                self._session_bus_cb, None)

    def _am_cb(self, am, result, user_data):
        am.prepare_finish(result)

        for account in am.get_valid_accounts():
            assert(account.object_path.startswith(Tp.ACCOUNT_OBJECT_PATH_BASE))

            tail = account.object_path[len(Tp.ACCOUNT_OBJECT_PATH_BASE):]
            self.accounts.append([account.get_display_name() or tail, account])

        self.account_combo.set_active(0)
        self.poll()

    def poll(self):
        if (self.am.is_prepared(ACCOUNT_MANAGER_FEATURE_CORE) and
            len(self.handlers) > 0 and
            len(self.potential_handlers) == 0):
            self.dial_button.props.sensitive = True

    def _session_bus_cb(self, session_bus, result, user_data):
        self.session_bus = Gio.bus_get_finish(result)

        self.session_bus.call('org.freedesktop.DBus',
                '/org/freedesktop/DBus',
                'org.freedesktop.DBus',
                'ListNames',
                GLib.Variant("()", ()),
                GLib.VariantType.new("(as)"),
                Gio.DBusCallFlags.NONE,
                -1,
                None,
                self._names_cb,
                None)

    def _names_cb(self, session_bus, result, user_data):
        names = set(session_bus.call_finish(result)[0])

        self.handlers.append(['(no preference)', ''])
        self.handler_combo.set_active(0)

        for name in sorted(names):
            if name.startswith(Tp.CLIENT_BUS_NAME_BASE):
                self.potential_handlers.add(name)
                Gio.DBusProxy.new(session_bus,
                        Gio.DBusProxyFlags.NONE,
                        None,
                        name,
                        '/' + name.replace('.', '/'),
                        Tp.IFACE_CLIENT_HANDLER,
                        None,
                        self._potential_handler_cb,
                        name)

        self.poll()

    def _potential_handler_cb(self, proxy, result, name):
        self.potential_handlers.remove(name)
        proxy = Gio.DBusProxy.new_finish(result)
        filters = proxy.get_cached_property("HandlerChannelFilter")
        if filters is not None:
            for asv in filters:
                if asv.get(Tp.PROP_CHANNEL_CHANNEL_TYPE) == Tp.IFACE_CHANNEL_TYPE_CALL:
                    tail = name[len(Tp.CLIENT_BUS_NAME_BASE):]
                    self.handlers.append([tail, name])
                    break

        self.poll()

    def _dial_clicked(self, button):
        tree_iter = self.account_combo.get_active_iter()
        account = self.accounts[tree_iter][1]
        tree_iter = self.handler_combo.get_active_iter()
        handler = self.handlers[tree_iter][1]

        acr = Tp.AccountChannelRequest.new(account,
                {
                    Tp.PROP_CHANNEL_CHANNEL_TYPE:
                        Tp.IFACE_CHANNEL_TYPE_CALL,
                    Tp.PROP_CHANNEL_TARGET_HANDLE_TYPE:
                        int(Tp.HandleType.CONTACT),
                    Tp.PROP_CHANNEL_TARGET_ID: self.target_entry.get_text(),
                    Tp.PROP_CHANNEL_TYPE_CALL_INITIAL_AUDIO: True,
                },
                USER_ACTION_TIME_NOT_USER_ACTION)
        acr.create_and_observe_channel_async(handler, None,
                self._create_call_cb, None)

    def _create_call_cb(self, acr, result, user_data):
        channel = acr.create_and_observe_channel_finish(result)
        print("accepting channel %s" % channel)
        channel.accept_async(self._accept_cb, None)

    def _accept_cb(self, channel, result, user_data):
        channel.accept_finish(result)
        print("accepted channel %s" % channel)

if __name__ == '__main__':
    Tp.debug_set_flags("all")

    sys.exit(Dialler().run(sys.argv))
