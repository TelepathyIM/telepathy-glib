#!/usr/bin/env python

import os

from gi.repository import GObject, Gio
from gi.repository import TelepathyGLib as Tp

def tube_conn_closed(tube, error):
    print "Tube connection has been closed", error.message

def tube_accept_cb(tube, result, loop):
    try:
        tube_conn = tube.accept_finish(result)
    except GObject.GError, e:
        print "Failed to accept tube: %s" % e
        sys.exit(1)

    tube_conn.connect('closed', tube_conn_closed)

    contact = tube_conn.get_contact();

    print "Got IOStream to", contact.get_identifier()

    conn = tube_conn.get_socket_connection();

    # g_input_stream_read() can't be used from Python so we use the more
    # binding friendly GDataInputStream
    in_stream = Gio.DataInputStream (base_stream=conn.get_input_stream())
    out_stream = conn.get_output_stream()

    print "Sending: Ping"
    out_stream.write("Ping\n", None)

    buf, len = in_stream.read_line_utf8(None)
    print "Received:", buf

def tube_invalidated_cb(tube, domain, code, message, loop):
    print "tube has been invalidated:", message
    loop.quit()

def handle_channels_cb(handler, account, connection, channels, requests,
    user_action_time, context, loop):
    for channel in channels:
        if not isinstance(channel, Tp.StreamTubeChannel):
            continue

        print "Accepting tube"

        channel.connect('invalidated', tube_invalidated_cb, loop)

        channel.accept_async(tube_accept_cb, loop)

    context.accept()

if __name__ == '__main__':
    Tp.debug_set_flags(os.getenv('EXAMPLE_DEBUG', ''))

    loop = GObject.MainLoop()

    account_manager = Tp.AccountManager.dup()
    handler = Tp.SimpleHandler.new_with_am(account_manager, False, False,
        'ExampleServiceHandler', False, handle_channels_cb, loop)

    handler.add_handler_filter({
        Tp.PROP_CHANNEL_CHANNEL_TYPE: Tp.IFACE_CHANNEL_TYPE_STREAM_TUBE,
        Tp.PROP_CHANNEL_TARGET_HANDLE_TYPE: int(Tp.HandleType.CONTACT),
        Tp.PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE: "ExampleService",
    })

    handler.register()

    print "Waiting for tube offer"
    loop.run()
