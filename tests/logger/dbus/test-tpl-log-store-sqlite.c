#include "config.h"

#include <telepathy-logger/log-store-sqlite-internal.h>
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/client-factory-internal.h>

int
main (int argc, char **argv)
{
  TplLogStore *store;
  TpDBusDaemon *bus;
  TpAccount *account;
  GError *error = NULL;
  TpClientFactory* factory;

  _tpl_debug_set_flags_from_env ();

  bus = tp_dbus_daemon_dup (&error);
  g_assert_no_error (error);

  factory = _tpl_client_factory_dup (bus);

  account =  tp_client_factory_ensure_account (factory,
      TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/danielle_2emadeley_40collabora_2eco_2euk0",
      NULL, &error);
  g_assert_no_error (error);

  store = _tpl_log_store_sqlite_dup ();

  g_print ("freq = %g\n",
      _tpl_log_store_sqlite_get_frequency (TPL_LOG_STORE_SQLITE (store),
        account, "dannielle.meyer@gmail.com"));

  g_object_unref (store);
  g_object_unref (account);
  g_object_unref (bus);
  g_object_unref (factory);
}
