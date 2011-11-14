#!/usr/bin/env python
import sys

from gi.repository import GObject
GObject.threads_init()

from gi.repository import Gio
from gi.repository import TelepathyGLib

import magic

def usage():
    print "%s FILE" % sys.argv[0]
    print "FILE is a path to the location you want the file saved to"

    sys.exit(1)

def state_changed_cb(channel, pspec, data):
    state, _ = channel.get_state()
    print 'State is now:', state

def accept_cb(channel, result, data):
    if not channel.accept_file_finish(result):
        print 'Failed to accept file'

def handle_channels_cb(handler, account, connection, channels, requests,
                       user_action_time, context, filename):

    for chan in channels:
        if not isinstance(chan, TelepathyGLib.FileTransferChannel):
            continue

        chan.connect('notify::state', state_changed_cb, None)

        print 'Handling FileTransfer channel:', chan.get_identifier()

        file = Gio.File.new_for_path(filename)
        chan.accept_file_async(file, 0, accept_cb, None)

    context.accept()

if __name__ == '__main__':
    if len(sys.argv) != 2:
        usage()

    _, filename = sys.argv

    #TelepathyGLib.debug_set_flags("all")

    dbus = TelepathyGLib.DBusDaemon.dup()

    handler = TelepathyGLib.SimpleHandler.new(dbus, False, False,
        'ExampleFTHandler', False, handle_channels_cb, filename)

    handler.add_handler_filter({
        TelepathyGLib.PROP_CHANNEL_CHANNEL_TYPE:
            TelepathyGLib.IFACE_CHANNEL_TYPE_FILE_TRANSFER,
        TelepathyGLib.PROP_CHANNEL_TARGET_HANDLE_TYPE:
            int(TelepathyGLib.HandleType.CONTACT),
        TelepathyGLib.PROP_CHANNEL_REQUESTED: False
        })

    handler.register()

    main_loop = GObject.MainLoop()

    main_loop.run()
