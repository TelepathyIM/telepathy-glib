/* Test CM parameters
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
  gchar *array_of_strings[] = { "Telepathy", "rocks", "!", NULL };
  guint i;
  GArray *array_of_bytes;
  guint8 bytes[] = { 0x1, 0x10, 0xA, 0xB, 0xC };
  GValue *value;

  array_of_bytes = g_array_new (FALSE, FALSE, sizeof (guint8));
  g_array_append_vals (array_of_bytes, bytes, sizeof (bytes));

  parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_hash_table_insert (parameters, "a-string",
      tp_g_value_slice_new_static_string ("a string"));
  g_hash_table_insert (parameters, "a-int16",
      tp_g_value_slice_new_int (G_MININT16));
  g_hash_table_insert (parameters, "a-int32",
      tp_g_value_slice_new_int (G_MININT32));
  g_hash_table_insert (parameters, "a-uint16",
      tp_g_value_slice_new_uint (G_MAXUINT16));
  g_hash_table_insert (parameters, "a-uint32",
      tp_g_value_slice_new_uint (G_MAXUINT32));
  g_hash_table_insert (parameters, "a-int64",
      tp_g_value_slice_new_int64 (G_MAXINT64));
  g_hash_table_insert (parameters, "a-uint64",
      tp_g_value_slice_new_uint64 (G_MAXUINT64));
  g_hash_table_insert (parameters, "a-boolean",
      tp_g_value_slice_new_boolean (TRUE));
  g_hash_table_insert (parameters, "a-double",
      tp_g_value_slice_new_double (G_MAXDOUBLE));
  g_hash_table_insert (parameters, "a-array-of-strings",
      tp_g_value_slice_new_static_boxed (G_TYPE_STRV, array_of_strings));
  g_hash_table_insert (parameters, "a-array-of-bytes",
      tp_g_value_slice_new_static_boxed (DBUS_TYPE_G_UCHAR_ARRAY,
        array_of_bytes));
  value = tp_g_value_slice_new (DBUS_TYPE_G_OBJECT_PATH);
  g_value_set_static_boxed (value, "/A/Object/Path");
  g_hash_table_insert (parameters, "a-object-path", value);

  tp_cli_connection_manager_run_request_connection (test->cm, -1,
      "example", parameters, NULL, NULL, &test->error, NULL);
  g_assert (test->error != NULL);
  g_assert (test->error->code == TP_ERROR_NOT_IMPLEMENTED);
  g_clear_error (&test->error);

  params = param_connection_manager_get_params_last_conn ();
  g_assert (params != NULL);

  g_assert (!tp_strdiff (params->a_string, "a string"));
  g_assert_cmpint (params->a_int16, ==, G_MININT16);
  g_assert_cmpint (params->a_int32, ==, G_MININT32);
  g_assert_cmpuint (params->a_uint16, ==, G_MAXUINT16);
  g_assert_cmpuint (params->a_uint32, ==, G_MAXUINT32);
  g_assert_cmpuint (params->a_int64, ==, G_MAXINT64);
  g_assert_cmpuint (params->a_uint64, ==, G_MAXUINT64);
  g_assert (params->a_boolean);
  g_assert_cmpfloat (params->a_double, ==, G_MAXDOUBLE);

  g_assert_cmpuint (g_strv_length (params->a_array_of_strings), ==,
      g_strv_length (array_of_strings));
  for (i = 0; array_of_strings[i] != NULL; i++)
    g_assert (!tp_strdiff (params->a_array_of_strings[i], array_of_strings[i]));

  g_assert_cmpuint (params->a_array_of_bytes->len, ==, array_of_bytes->len);
  for (i = 0; i < array_of_bytes->len; i++)
    g_assert (params->a_array_of_bytes->data[i] == array_of_bytes->data[i]);

  g_assert (!tp_strdiff (params->a_object_path, "/A/Object/Path"));

  param_connection_manager_free_params (params);
  g_hash_table_destroy (parameters);
  g_array_free (array_of_bytes, TRUE);
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
