#! /usr/bin/gjs

const Tp = imports.gi.TelepathyGLib;
const Mainloop = imports.mainloop;

let manager = Tp.AccountManager.dup();
let factory = manager.get_factory();
factory.add_account_features([Tp.Account.get_feature_quark_connection()]);
factory.add_connection_features([Tp.Connection.get_feature_quark_contact_list()]);
factory.add_contact_features([Tp.ContactFeature.CONTACT_GROUPS]);

manager.prepare_async(null, function(self, result) {
    manager.prepare_finish(result);

    let accounts = manager.get_valid_accounts();
    for (let i = 0; i < accounts.length; i++) {
        let connection = accounts[i].get_connection();

        if (connection != null &&
            connection.get_contact_list_state() == Tp.ContactListState.SUCCESS) {

            let contacts = connection.dup_contact_list();
            for (let j = 0; j < contacts.length; j++) {
                let contact = contacts[j];
                // poor man's printf...
                print (contact.get_identifier() + ' (' + contact.get_contact_groups() + ')');
            }
        }
    }
    Mainloop.quit('example');
});

Mainloop.run('example');
