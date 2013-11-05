#! /usr/bin/gjs
//
// A simple Telepathy debug-message collector.
//
// Copyright © 2013 Intel Corporation
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Tp = imports.gi.TelepathyGLib;

// this is just a subset
const DBusDaemonIface = <interface name='org.freedesktop.DBus'>
    <method name='ListNames'>
        <arg type='as' direction='out'/>
    </method>
    <method name='GetNameOwner'>
        <arg type='s' direction='in'/>
        <arg type='s' direction='out'/>
    </method>
    <signal name="NameOwnerChanged">
        <arg type="s"/>
        <arg type="s"/>
        <arg type="s"/>
    </signal>
</interface>;
const DBusDaemonProxy = Gio.DBusProxy.makeProxyWrapper(DBusDaemonIface);

function levelStr(level) {
    bits = []

    if (level & GLib.LogLevelFlags.FLAG_FATAL)
        bits.push('FATAL')

    if (level & GLib.LogLevelFlags.LEVEL_ERROR)
        bits.push('ERROR')

    if (level & GLib.LogLevelFlags.LEVEL_CRITICAL)
        bits.push('CRITICAL')

    if (level & GLib.LogLevelFlags.LEVEL_WARNING)
        bits.push('WARNING')

    if (level & GLib.LogLevelFlags.LEVEL_MESSAGE)
        bits.push('MESSAGE')

    if (level & GLib.LogLevelFlags.LEVEL_INFO)
        bits.push('INFO')

    if (level & GLib.LogLevelFlags.LEVEL_DEBUG)
        bits.push('DEBUG')

    if (level & GLib.LogLevelFlags.FLAG_RECURSION)
        bits.push('RECURSION')

    if (bits.length > 0) {
        return bits.join('|')
    } else {
        return '(level?)'
    }
}

function zeroPad(x) {
    let s = x.toString();

    while (s.length < 6)
        s = '0' + s;

    return s;
}

DebugClient = new Lang.Class({
    Name: 'DebugClient',
    Extends: Gio.Application,

    _init: function() {
        this.parent({application_id: 'im.telepathy.TpGLib.Example.DebugClient',
            flags: Gio.ApplicationFlags.NON_UNIQUE });

        this.json = false;
        this._gdbus = Gio.DBus.session;
        this._dbusDaemon = Tp.DBusDaemon.dup();
        this._debuggables = {};
        this._timeOrigin = null;

        this._gdbusProxy = new DBusDaemonProxy(Gio.DBus.session,
            'org.freedesktop.DBus', '/org/freedesktop/DBus');
        this._gdbusProxy.connectSignal('NameOwnerChanged',
            Lang.bind(this, this._onNameOwnerChanged));
        this._gdbusProxy.ListNamesRemote(Lang.bind(this, this._listNamesCb));
    },

    vfunc_activate: function() {
        this.hold();
    },

    _setEnabledCb: function(debugClient, result) {
        try {
            debugClient.set_enabled_finish(result);
        } catch (e) {
            logError(e, 'Unable to enable debug client ' +
                    debugClient.get_bus_name());
        }
    },

    _getMessagesCb: function(debugClient, result) {
        let messages;
        try {
            messages = debugClient.get_messages_finish(result);
        } catch (e) {
            logError(e, 'Unable to enable debug client ' +
                    debugClient.get_bus_name());
            return;
        }
        for (i in messages) {
            this._newDebugMessageCb(debugClient, messages[i]);
        }
    },

    _newDebugMessageCb: function(debugClient, debugMessage) {
        let dateTime = debugMessage.get_time();

        if (!this._timeOrigin)
            this._timeOrigin = dateTime;

        if (this.json) {
            print(JSON.stringify({
                time: dateTime.format('%Y-%m-%d %H:%M:%S'),
                usec: dateTime.difference(this._timeOrigin),
                uniqueName: debugClient.get_bus_name(),
                domain: debugMessage.get_domain(),
                category: debugMessage.get_category(),
                level: debugMessage.get_level(),
                message: debugMessage.get_message(),
                stamp: (dateTime.to_unix() * 1000000) + dateTime.get_microsecond(),
            }));
        } else {
            print(dateTime.format('%Y-%m-%d %H:%M:%S.') +
                    zeroPad(dateTime.get_microsecond(), 6) + ' ' +
                    debugClient.get_bus_name() + ' ' +
                    debugMessage.get_domain() + '/' +
                    debugMessage.get_category() + ' ' +
                    levelStr(debugMessage.get_level()) + ': ' +
                    debugMessage.get_message());
        }
    },

    _addDebuggable: function(wellKnownName, owner) {
        if (owner == null) {
            this._gdbusProxy.GetNameOwnerRemote(wellKnownName,
                    Lang.bind(this, function(result, error) {
                        if (error) {
                            logError(error);
                        } else if (result[0] != null) {
                            print(result[0] + ' owns well-known name ' +
                                    wellKnownName);
                            this._addDebuggable(wellKnownName, result[0]);
                        } else {
                            log('owner of ' + wellKnownName + ' is null?!');
                        }
                    }));
            return;
        }

        if (owner in this._debuggables ||
            owner == this._dbusDaemon.get_unique_name() ||
            owner == this._gdbus.get_unique_name())
            return;

        debugClient = Tp.DebugClient.new(this._dbusDaemon, owner);
        this._debuggables[owner] = debugClient;

        debugClient.connect('new-debug-message',
                Lang.bind(this, this._newDebugMessageCb));
        debugClient.set_enabled_async(true,
                Lang.bind(this, this._setEnabledCb), null);
        debugClient.get_messages_async(Lang.bind(this, this._getMessagesCb));
    },

    _isInteresting: function(wellKnownName) {
        return (GLib.str_has_prefix(wellKnownName,
                    'im.telepathy.v1'));

    },

    _onNameOwnerChanged: function(proxy, sender, [wellKnownName, oldOwner,
                                 newOwner]) {
        if (this._isInteresting(wellKnownName) && newOwner != '') {
            print(newOwner + ' owns well-known name ' + wellKnownName);
            this._addDebuggable(wellKnownName, newOwner);
        }
    },

    _listNamesCb: function(result, error) {
        if (error) {
            logError(error, 'Unable to list bus names');
            return;
        }
        for (i in result[0]) {
            let wellKnownName = result[0][i];
            if (this._isInteresting(wellKnownName)) {
                this._addDebuggable(wellKnownName, null);
            }
        }
    },
});

let main = function () {
    let app = new DebugClient();

    for (i in ARGV) {
        if (ARGV[i] == '--json')
            app.json = true;
        else {
            printerr('Usage: telepathy-debug-client [--json]');
            return 1;
        }
    }

    return app.run(ARGV);
};

main();
