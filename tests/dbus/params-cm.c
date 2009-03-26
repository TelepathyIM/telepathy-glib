/* Feature test for example StreamedMedia CM code.
 *
 * Copyright © 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2009 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#include "tests/lib/util.h"
#include "tests/lib/params-cm.h"

#define CLEAR_OBJECT(o) \
  G_STMT_START { \
      if (*(o) != NULL) \
        { \
          g_object_unref (*(o)); \
          *(o) = NULL; \
        } \
  } G_STMT_END

typedef struct
{
  GMainLoop *mainloop;
  TpDBusDaemon *dbus;
  GError *error /* statically initialized to NULL */ ;

  ParamConnectionManager *service_cm;

  TpConnectionManager *cm;
  TpConnection *conn;
} Test;

static void
cm_ready_cb (TpConnectionManager *cm G_GNUC_UNUSED,
             const GError *error,
             gpointer user_data,
             GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  test_assert_no_error (error);
  g_main_loop_quit (test->mainloop);
}

static void
setup (Test *test,
       gconstpointer data G_GNUC_UNUSED)
{
  TpBaseConnectionManager *service_cm_as_base;
  gboolean ok;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_dbus_daemon_dup (NULL);
  g_assert (test->dbus != NULL);

  test->service_cm = PARAM_CONNECTION_MANAGER (g_object_new (
        TYPE_PARAM_CONNECTION_MANAGER,
        NULL));
  g_assert (test->service_cm != NULL);
  service_cm_as_base = TP_BASE_CONNECTION_MANAGER (test->service_cm);
  g_assert (service_cm_as_base != NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  test->cm = tp_connection_manager_new (test->dbus, "params_cm",
      NULL, &test->error);
  g_assert (test->cm != NULL);
  tp_connection_manager_call_when_ready (test->cm, cm_ready_cb, test, NULL,
      NULL);
  g_main_loop_run (test->mainloop);
}

static void
teardown (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  CLEAR_OBJECT (&test->cm);
  CLEAR_OBJECT (&test->service_cm);

  CLEAR_OBJECT (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
test_set_params (Test *test,
                 gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *parameters;
  CMParams *params;

  parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_hash_table_insert (parameters, "a-string",
      tp_g_value_slice_new_static_string ("a string"));
  g_hash_table_insert (parameters, "a-int16",
      tp_g_value_slice_new_int (7));
  g_hash_table_insert (parameters, "a-int32",
      tp_g_value_slice_new_int (77));

  tp_cli_connection_manager_run_request_connection (test->cm, -1,
      "example", parameters, NULL, NULL, &test->error, NULL);
  g_assert (test->error != NULL);
  g_assert (test->error->code == TP_ERROR_NOT_IMPLEMENTED);

  params = param_connection_manager_get_params_last_conn ();
  g_assert (params != NULL);

  g_assert (!tp_strdiff (params->a_string, "a string"));
  g_assert_cmpuint (params->a_int16, ==, 7);
  g_assert_cmpuint (params->a_int32, ==, 77);

  free_cm_params (params);
  g_hash_table_destroy (parameters);
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/params-cm/set-params", Test, NULL, setup, test_set_params,
      teardown);

  return g_test_run ();
}
