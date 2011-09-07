#!/usr/bin/env python

import os
from gi.repository import GObject
GObject.threads_init()

from gi.repository import TelepathyGLib as Tp

def manager_prepared_cb(manager, result, loop):
    manager.prepare_finish(result)

    for account in manager.get_valid_accounts():
        connection = account.get_connection()

        # Verify account is online and received its contact list. If state is not
        # SUCCESS this means we didn't received the roster from server yet and
        # we would have to wait for the "notify:contact-list-state" signal. */
        if connection is not None and \
           connection.get_contact_list_state() == Tp.ContactListState.SUCCESS:
            contacts = connection.dup_contact_list()
            for contact in contacts:
                print "%s (%s)" % (contact.get_identifier(), contact.get_contact_groups())
    loop.quit()

if __name__ == '__main__':
    Tp.debug_set_flags(os.getenv('EXAMPLE_DEBUG', ''))

    loop = GObject.MainLoop()
    manager = Tp.AccountManager.dup()
    factory = manager.get_factory()
    factory.add_account_features([Tp.Account.get_feature_quark_connection()])
    factory.add_connection_features([Tp.Connection.get_feature_quark_contact_list()])
    factory.add_contact_features([Tp.ContactFeature.CONTACT_GROUPS])

    manager.prepare_async(None, manager_prepared_cb, loop)
    loop.run()
