#!/usr/bin/env python

import sys
import os

from gi.repository import GObject, Gio
from gi.repository import TelepathyGLib as Tp

def usage():
    print "%s ACCOUNT CONTACT" % sys.argv[0]
    print "ACCOUNT is a Telepathy account name, use 'mc-tool list' to list all your accounts"
    print "CONTACT is a contact id such as badger@nyan.cat"

    sys.exit(1)

def offer_channel_cb(tube, result, loop):
    try:
        tube.offer_finish(result)
        print "tube offered"

    except GObject.GError, e:
        print "Failed to offer tube: %s" % e
        sys.exit(1)

def tube_conn_closed(tube, error):
    print "Tube connection has been closed", error.message

def channel_close_cb(tube, result, loop):
    try:
        tube.close_finish(result)
        print "tube channel closed"

    except GObject.GError, e:
        print "Failed to close tube channel: %s" % e
        sys.exit(1)

def tube_incoming_cb(tube, tube_conn, loop):
    tube_conn.connect('closed', tube_conn_closed)

    contact = tube_conn.get_contact();

    print "Got IOStream from", contact.get_identifier()

    conn = tube_conn.get_socket_connection();

    # g_input_stream_read() can't be used from Python so we use the more
    # binding friendly GDataInputStream
    in_stream = Gio.DataInputStream (base_stream=conn.get_input_stream())
    out_stream = conn.get_output_stream()

    buf, len = in_stream.read_line_utf8(None)
    print "Received:", buf

    print "Sending: Pong"
    out_stream.write("Pong\n", None)

    tube.close_async(channel_close_cb, contact)

def tube_invalidated_cb(tube, domain, code, message, loop):
    print "tube has been invalidated:", message
    loop.quit()

def create_channel_cb(request, result, loop):
    try:
        (chan, context) = request.create_and_handle_channel_finish(result)

        chan.connect('incoming', tube_incoming_cb, loop)
        chan.connect('invalidated', tube_invalidated_cb, loop)

        chan.offer_async({}, offer_channel_cb, loop)

    except GObject.GError, e:
        print "Failed to create channel: %s" % e
        sys.exit(1)

if __name__ == '__main__':
    Tp.debug_set_flags(os.getenv('EXAMPLE_DEBUG', ''))

    if len(sys.argv) != 3:
        usage()

    _, account_id, contact_id = sys.argv

    account_manager = Tp.AccountManager.dup()
    account = account_manager.ensure_account("%s%s" %
        (Tp.ACCOUNT_OBJECT_PATH_BASE, account_id))

    request_dict = {
        Tp.PROP_CHANNEL_CHANNEL_TYPE:
            Tp.IFACE_CHANNEL_TYPE_STREAM_TUBE,
        Tp.PROP_CHANNEL_TARGET_HANDLE_TYPE:
            int(Tp.HandleType.CONTACT),
        Tp.PROP_CHANNEL_TARGET_ID:
            contact_id,

        Tp.PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE: "ExampleService",
        }

    request = Tp.AccountChannelRequest.new(account, request_dict, 0)

    loop = GObject.MainLoop()
    request.create_and_handle_channel_async(None, create_channel_cb, loop)

    loop.run()
