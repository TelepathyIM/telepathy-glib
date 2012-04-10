#!/usr/bin/env python
import sys
import os
import mimetypes

from gi.repository import GObject
GObject.threads_init()

from gi.repository import Gio
from gi.repository import TelepathyGLib

def usage():
    print "%s ACCOUNT CONTACT FILE" % sys.argv[0]
    print "ACCOUNT is a Telepathy account name, use 'mc-tool list' to list all your accounts"
    print "CONTACT is a contact id such as badger@nyan.cat"
    print "FILE is a path to the local file you want sent"

    sys.exit(1)

def provide_file_cb(channel, result, data):
    if not channel.provide_file_finish(result):
        print 'failed to provide'

def state_changed_cb(channel, pspec, data):
    main_loop, file_path = data
    state, _ = channel.get_state()
    print 'state is now', state

    if state == TelepathyGLib.FileTransferState.ACCEPTED:
        file = Gio.File.new_for_path(file_path)
        channel.provide_file_async(file, provide_file_cb, None)

def create_channel_cb(request, result, data):
    try:
        (chan, context) = request.create_and_handle_channel_finish(result)

        chan.connect('notify::state', state_changed_cb, data)
    except GObject.GError, e:
        print "Failed to create channel: %s" % e
        sys.exit(1)

if __name__ == '__main__':
    #TelepathyGLib.debug_set_flags("all")

    dbus = TelepathyGLib.DBusDaemon.dup()

    if len(sys.argv) != 4:
        usage()

    _, account_id, contact_id, file_path = sys.argv

    account_manager = TelepathyGLib.AccountManager.dup()
    account = account_manager.ensure_account("%s%s" %
        (TelepathyGLib.ACCOUNT_OBJECT_PATH_BASE, account_id))

    # Get file info stuff
    (mimetype, encoding) = mimetypes.guess_type(file_path)
    mtime = os.path.getmtime(file_path)
    filename = os.path.basename(file_path)
    filesize = os.path.getsize(file_path)

    request_dict = {
        TelepathyGLib.PROP_CHANNEL_CHANNEL_TYPE:
            TelepathyGLib.IFACE_CHANNEL_TYPE_FILE_TRANSFER,
        TelepathyGLib.PROP_CHANNEL_TARGET_HANDLE_TYPE:
            int(TelepathyGLib.HandleType.CONTACT),
        TelepathyGLib.PROP_CHANNEL_TARGET_ID:
            contact_id,

        TelepathyGLib.PROP_CHANNEL_TYPE_FILE_TRANSFER_CONTENT_TYPE:
            mimetype,
        TelepathyGLib.PROP_CHANNEL_TYPE_FILE_TRANSFER_DATE:
            mtime,
        TelepathyGLib.PROP_CHANNEL_TYPE_FILE_TRANSFER_DESCRIPTION:
            "",
        TelepathyGLib.PROP_CHANNEL_TYPE_FILE_TRANSFER_FILENAME:
            filename,
        TelepathyGLib.PROP_CHANNEL_TYPE_FILE_TRANSFER_INITIAL_OFFSET:
            0,
        TelepathyGLib.PROP_CHANNEL_TYPE_FILE_TRANSFER_SIZE:
            filesize,
        }

    request = TelepathyGLib.AccountChannelRequest.new(account, request_dict, 0)

    main_loop = GObject.MainLoop()

    request.create_and_handle_channel_async(None, create_channel_cb, (main_loop, file_path))

    main_loop.run()
