#!/usr/bin/env python

from gi.repository import GObject
GObject.threads_init()

from gi.repository import TelepathyGLib

def echo_message(channel, msg, pending):
    text, flags = msg.to_text()

    if pending:
        print "pending: %s" % (text)
    else:
        print "received: %s" % (text)

    reply = TelepathyGLib.ClientMessage.new_text(
        TelepathyGLib.ChannelTextMessageType.NORMAL, text.upper())

    channel.send_message_async(reply, 0, lambda a, b, c: 0, None)

def message_received_cb(channel, msg):
    echo_message(channel, msg, False)

    channel.ack_message_async(msg, lambda a, b, c: 0, None)

def display_pending_messages(channel):
    messages = channel.get_pending_messages()

    for msg in messages:
        echo_message(channel, msg, True)

    # Ideally we should pass None as callback but that doesn't work
    # (bgo #640812)
    channel.ack_messages_async(messages, lambda a, b, c: 0, None)

def handle_channels_cb(handler, account, connection, channels, requests,
    user_action_time, context, user_data):
    for channel in channels:
        if not isinstance(channel, TelepathyGLib.TextChannel):
            continue

        print "Handling text channel with", channel.get_identifier()

        channel.connect('message-received', message_received_cb)

        display_pending_messages(channel)

    context.accept()

if __name__ == '__main__':
    #TelepathyGLib.debug_set_flags("all")

    dbus = TelepathyGLib.DBusDaemon.dup()

    handler = TelepathyGLib.SimpleHandler.new(dbus, False, False,
        'ExampleHandler', False, handle_channels_cb, None)

    handler.add_handler_filter({
        TelepathyGLib.PROP_CHANNEL_CHANNEL_TYPE: TelepathyGLib.IFACE_CHANNEL_TYPE_TEXT,
        # bgo #637466
        TelepathyGLib.PROP_CHANNEL_TARGET_HANDLE_TYPE: int(TelepathyGLib.HandleType.CONTACT),
        TelepathyGLib.PROP_CHANNEL_REQUESTED: False,
    })

    handler.register()

    main_loop = GObject.MainLoop()
    main_loop.run()
