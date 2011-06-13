#!/usr/bin/env python

import os
import gobject
gobject.threads_init()

from gi.repository import TelepathyGLib

def manager_prepared_cb(manager, result, loop):
    manager.prepare_finish(result)

    for account in manager.get_valid_accounts():
        connection = account.get_connection()
        if connection is not None:
            contacts = connection.dup_contact_list()
            for contact in contacts:
                print "%s (%s)" % (contact.get_identifier(), contact.get_contact_groups())
    loop.quit()

if __name__ == '__main__':
    TelepathyGLib.debug_set_flags(os.getenv('EXAMPLE_DEBUG', ''))

    loop = gobject.MainLoop()
    manager = TelepathyGLib.AccountManager.dup()
    factory = manager.get_factory()
    factory.add_account_features([TelepathyGLib.Account.get_feature_quark_connection()])
    factory.add_connection_features([TelepathyGLib.Connection.get_feature_quark_contact_list()])
    factory.add_contact_features([TelepathyGLib.ContactFeature.CONTACT_GROUPS])

    manager.prepare_async(None, manager_prepared_cb, loop)
    loop.run()
