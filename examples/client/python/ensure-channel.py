#!/usr/bin/env python
import sys

from gi.repository import GObject
GObject.threads_init()

from gi.repository import TelepathyGLib

def usage():
    print "%s ACCOUNT [text|audio|video] CONTACT" % sys.argv[0]
    print "ACCOUNT is a Telepathy account name, use 'mc-tool list' to list all your accounts"
    print "CONTACT is a contact id such as badger@gmail.com"

    sys.exit(1)

def create_request_dict(action, contact_id):
    if action == 'text':
        return {
            TelepathyGLib.PROP_CHANNEL_CHANNEL_TYPE:
                TelepathyGLib.IFACE_CHANNEL_TYPE_TEXT,
            TelepathyGLib.PROP_CHANNEL_TARGET_HANDLE_TYPE:
                int(TelepathyGLib.HandleType.CONTACT),
            TelepathyGLib.PROP_CHANNEL_TARGET_ID: contact_id}
    elif action in ['audio', 'video']:
        return {
            TelepathyGLib.PROP_CHANNEL_CHANNEL_TYPE:
                TelepathyGLib.IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
            TelepathyGLib.PROP_CHANNEL_TARGET_HANDLE_TYPE:
                int(TelepathyGLib.HandleType.CONTACT),
            TelepathyGLib.PROP_CHANNEL_TARGET_ID: contact_id,
            TelepathyGLib.PROP_CHANNEL_TYPE_STREAMED_MEDIA_INITIAL_AUDIO:
                True,
            TelepathyGLib.PROP_CHANNEL_TYPE_STREAMED_MEDIA_INITIAL_VIDEO:
                action == 'video'}
    else:
        usage()

def ensure_channel_cb(request, result, main_loop):
    request.ensure_channel_finish(result)

    main_loop.quit()

if __name__ == '__main__':
    #TelepathyGLib.debug_set_flags("all")

    dbus = TelepathyGLib.DBusDaemon.dup()

    if len(sys.argv) != 4:
        usage()

    _, account_id, action, contact_id = sys.argv

    account_manager = TelepathyGLib.AccountManager.dup()

    account = account_manager.ensure_account("%s%s" %
        (TelepathyGLib.ACCOUNT_OBJECT_PATH_BASE, account_id))

    request_dict = create_request_dict(action, contact_id)

    request = TelepathyGLib.AccountChannelRequest.new(account, request_dict, 0)
    # FIXME: for some reason TelepathyGLib.USER_ACTION_TIME_CURRENT_TIME is
    # not defined (bgo #639206)

    main_loop = GObject.MainLoop()

    request.ensure_channel_async("", None, ensure_channel_cb, main_loop)

    main_loop.run()
