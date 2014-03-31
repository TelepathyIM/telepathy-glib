#include "config.h"

#include <telepathy-glib/telepathy-glib.h>

#include <telepathy-logger/debug.h>
#include <telepathy-logger/log-store-sqlite-internal.h>
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/client-factory-internal.h>

#include "tests/lib/util.h"

typedef struct {
    int dummy;
} Fixture;

static void
setup (Fixture *fixture,
    gconstpointer data)
{
  tpl_debug_set_flags ("all");
  tp_debug_set_flags ("all");
}

static void
test (Fixture *fixture,
    gconstpointer data)
{
  TplLogStore *store;
  GDBusConnection *bus;
  TpAccount *account;
  GError *error = NULL;
  TpClientFactory* factory;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
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

static void
teardown (Fixture *fixture,
    gconstpointer data)
{
}

int
main (int argc,
    char **argv)
{
  tp_tests_init (&argc, &argv);

  g_test_add ("/log-store-sqlite", Fixture, NULL, setup, test, teardown);

  return tp_tests_run_with_bus ();
}
