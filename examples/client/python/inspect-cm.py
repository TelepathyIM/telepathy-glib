#!/usr/bin/env python

import sys
from gi.repository import GObject
from gi.repository import TelepathyGLib as Tp

def describe(cm):
    print("Connection manager: %s" % cm.get_name())
    print("")

    for protocol in cm.dup_protocols():
        print("Protocol: %s" % protocol.get_name())
        print("\tEnglish name: %s" % protocol.get_english_name())
        print("\tvCard field: %s" % protocol.get_vcard_field())

        for param in protocol.dup_params():
            print("\tParameter: %s" % param.get_name())

            if param.is_required():
                print("\t\tIs required")

            if param.is_secret():
                print("\t\tIs a password or equivalent")

            if param.flags & Tp.ConnMgrParamFlags.HAS_DEFAULT:
                print("\t\tDefault value: %s" % param.default_value)
            else:
                print("\t\tNo default")

def manager_prepared_cb(cm, result, loop):
    cm.prepare_finish(result)
    describe(cm)
    loop.quit()

def inspect(name):
    cm = Tp.ConnectionManager(
            dbus_daemon=Tp.DBusDaemon.dup(),
            bus_name=Tp.CM_BUS_NAME_BASE + name,
            object_path=Tp.CM_OBJECT_PATH_BASE + name,
            )
    cm.prepare_async(None, cm, loop)

def cms_cb(source, result, loop):
    cms = Tp.list_connection_managers_finish(result)

    for cm in cms:
        describe(cm)
        print("")

    loop.quit()

if __name__ == '__main__':
    loop = GObject.MainLoop()

    if len(sys.argv) >= 2:
        inspect(sys.argv[1])
    else:
        Tp.list_connection_managers_async(Tp.DBusDaemon.dup(), cms_cb, loop)

    loop.run()
