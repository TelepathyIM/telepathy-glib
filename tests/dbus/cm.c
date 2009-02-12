/* Feature test for https://bugs.freedesktop.org/show_bug.cgi?id=18291
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/debug.h>

#include "examples/cm/echo/connection-manager.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;
    ExampleEchoConnectionManager *service_cm;

    TpConnectionManager *cm;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  TpBaseConnectionManager *service_cm_as_base;
  gboolean ok;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_dbus_daemon_dup (NULL);
  g_assert (test->dbus != NULL);

  test->service_cm = EXAMPLE_ECHO_CONNECTION_MANAGER (g_object_new (
        EXAMPLE_TYPE_ECHO_CONNECTION_MANAGER,
        NULL));
  g_assert (test->service_cm != NULL);
  service_cm_as_base = TP_BASE_CONNECTION_MANAGER (test->service_cm);
  g_assert (service_cm_as_base != NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  test->cm = NULL;
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_object_unref (test->service_cm);
  test->service_cm = NULL;
  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
test_valid_name (void)
{
  GError *error = NULL;

  g_assert (tp_connection_manager_check_valid_name ("gabble", NULL));

  g_assert (tp_connection_manager_check_valid_name ("l33t_cm", NULL));

  g_assert (!tp_connection_manager_check_valid_name ("wtf tbh", &error));
  g_assert (error != NULL);
  g_clear_error (&error);

  g_assert (!tp_connection_manager_check_valid_name ("0pointer", &error));
  g_assert (error != NULL);
  g_clear_error (&error);
}

static void
test_file (Test *test,
           gconstpointer data)
{
  GError *error = NULL;

  test->cm = tp_connection_manager_new (test->dbus, "test_manager_file",
      NULL, &error);
  g_assert (error == NULL);
  g_test_queue_unref (test->cm);
}

static void
test_dbus (Test *test,
           gconstpointer data)
{
  GError *error = NULL;

  test->cm = tp_connection_manager_new (test->dbus,
      TP_BASE_CONNECTION_MANAGER_GET_CLASS (test->service_cm)->cm_dbus_name,
      NULL, &error);
  g_assert (error == NULL);
  g_test_queue_unref (test->cm);
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/cm/valid-name", test_valid_name);
  g_test_add ("/cm/file", Test, NULL, setup, test_file, teardown);
  g_test_add ("/cm/dbus", Test, NULL, setup, test_dbus, teardown);

  return g_test_run ();
}
