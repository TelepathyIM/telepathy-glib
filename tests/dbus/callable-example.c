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

#include "examples/cm/callable/connection-manager.h"
#include "examples/cm/callable/conn.h"
#include "examples/cm/callable/media-channel.h"
#include "examples/cm/callable/media-stream.h"

#include "tests/lib/util.h"

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

  ExampleCallableConnectionManager *service_cm;

  TpConnectionManager *cm;
  TpConnection *conn;
  TpChannel *chan;
  TpHandle self_handle;

  GPtrArray *request_streams_return;
  GPtrArray *list_streams_return;
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
conn_ready_cb (TpConnection *conn G_GNUC_UNUSED,
               const GError *error,
               gpointer user_data)
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
  gchar *bus_name;
  gchar *object_path;
  GHashTable *parameters;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_dbus_daemon_dup (NULL);
  g_assert (test->dbus != NULL);

  test->service_cm = EXAMPLE_CALLABLE_CONNECTION_MANAGER (g_object_new (
        EXAMPLE_TYPE_CALLABLE_CONNECTION_MANAGER,
        NULL));
  g_assert (test->service_cm != NULL);
  service_cm_as_base = TP_BASE_CONNECTION_MANAGER (test->service_cm);
  g_assert (service_cm_as_base != NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  test->cm = tp_connection_manager_new (test->dbus, "example_callable",
      NULL, &test->error);
  g_assert (test->cm != NULL);
  tp_connection_manager_call_when_ready (test->cm, cm_ready_cb, test, NULL,
      NULL);
  g_main_loop_run (test->mainloop);

  parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_hash_table_insert (parameters, "account",
      tp_g_value_slice_new_static_string ("me"));

  tp_cli_connection_manager_run_request_connection (test->cm, -1,
      "example", parameters, &bus_name, &object_path, &test->error, NULL);
  test_assert_no_error (test->error);

  test->conn = tp_connection_new (test->dbus, bus_name, object_path,
      &test->error);
  test_assert_no_error (test->error);
  g_assert (test->conn != NULL);
  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);
  tp_connection_call_when_ready (test->conn, conn_ready_cb, test);
  g_main_loop_run (test->mainloop);

  test->self_handle = tp_connection_get_self_handle (test->conn);
  g_assert (test->self_handle != 0);

  g_free (bus_name);
  g_free (object_path);
}

static void
channel_created_cb (TpConnection *connection,
                    const gchar *object_path,
                    GHashTable *immutable_properties,
                    const GError *error,
                    gpointer user_data,
                    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  GError *new_error = NULL;

  test_assert_no_error (error);

  test->chan = tp_channel_new_from_properties (connection, object_path,
      immutable_properties, &new_error);
  test_assert_no_error (new_error);

  g_main_loop_quit (test->mainloop);
}

static void
channel_ready_cb (TpChannel *channel G_GNUC_UNUSED,
                  const GError *error,
                  gpointer user_data)
{
  Test *test = user_data;

  test_assert_no_error (error);
  g_main_loop_quit (test->mainloop);
}

static void
requested_streams_cb (TpChannel *chan,
                      const GPtrArray *stream_info,
                      const GError *error,
                      gpointer user_data,
                      GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  if (error != NULL)
    {
      test->error = g_error_copy (error);
    }
  else
    {
      test->request_streams_return = g_boxed_copy (
          TP_ARRAY_TYPE_MEDIA_STREAM_INFO_LIST, stream_info);
    }

  g_main_loop_quit (test->mainloop);
}

static void
listed_streams_cb (TpChannel *chan,
                   const GPtrArray *stream_info,
                   const GError *error,
                   gpointer user_data,
                   GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  /* ListStreams shouldn't fail in any of these tests */
  test_assert_no_error (error);

  test->list_streams_return = g_boxed_copy (
      TP_ARRAY_TYPE_MEDIA_STREAM_INFO_LIST, stream_info);

  g_main_loop_quit (test->mainloop);
}

static void
test_basics (Test *test,
             gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) tp_g_value_slice_free);
  GArray *audio_request = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
  guint audio = TP_MEDIA_STREAM_TYPE_AUDIO;
  GValueArray *va;
  guint audio_stream_id;

  g_array_append_val (audio_request, audio);

  g_hash_table_insert (request, TP_IFACE_CHANNEL ".ChannelType",
      tp_g_value_slice_new_static_string (
        TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA));
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetHandleType",
      tp_g_value_slice_new_uint (TP_HANDLE_TYPE_CONTACT));
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetID",
      tp_g_value_slice_new_static_string ("basic-test"));

  tp_cli_connection_interface_requests_call_create_channel (test->conn, -1,
      request, channel_created_cb, test, NULL, NULL);
  g_hash_table_destroy (request);
  request = NULL;
  g_main_loop_run (test->mainloop);

  tp_channel_call_when_ready (test->chan, channel_ready_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert_cmpuint (tp_channel_group_get_self_handle (test->chan), ==,
      test->self_handle);

  /* RequestStreams */

  tp_cli_channel_type_streamed_media_call_request_streams (test->chan, -1,
      tp_channel_get_handle (test->chan, NULL),
      audio_request, requested_streams_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  g_assert_cmpuint (test->request_streams_return->len, ==, 1);
  va = g_ptr_array_index (test->request_streams_return, 0);

  g_assert (G_VALUE_HOLDS_UINT (va->values + 0));
  g_assert (G_VALUE_HOLDS_UINT (va->values + 1));
  g_assert (G_VALUE_HOLDS_UINT (va->values + 2));
  g_assert (G_VALUE_HOLDS_UINT (va->values + 3));
  g_assert (G_VALUE_HOLDS_UINT (va->values + 4));
  g_assert (G_VALUE_HOLDS_UINT (va->values + 5));

  audio_stream_id = g_value_get_uint (va->values + 0);

  g_assert_cmpuint (g_value_get_uint (va->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (va->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);
  g_assert_cmpuint (g_value_get_uint (va->values + 3), ==,
      TP_MEDIA_STREAM_STATE_DISCONNECTED);
  g_assert_cmpuint (g_value_get_uint (va->values + 4), ==,
      TP_MEDIA_STREAM_DIRECTION_NONE);
  g_assert_cmpuint (g_value_get_uint (va->values + 5), ==, 0);

  /* ListStreams */

  tp_cli_channel_type_streamed_media_call_list_streams (test->chan, -1,
      listed_streams_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  g_assert_cmpuint (test->list_streams_return->len, ==, 1);
  va = g_ptr_array_index (test->list_streams_return, 0);

  g_assert (G_VALUE_HOLDS_UINT (va->values + 0));
  g_assert (G_VALUE_HOLDS_UINT (va->values + 1));
  g_assert (G_VALUE_HOLDS_UINT (va->values + 2));
  g_assert (G_VALUE_HOLDS_UINT (va->values + 3));
  g_assert (G_VALUE_HOLDS_UINT (va->values + 4));
  g_assert (G_VALUE_HOLDS_UINT (va->values + 5));

  /* value 0 is the handle */
  g_assert_cmpuint (g_value_get_uint (va->values + 0), ==, audio_stream_id);
  g_assert_cmpuint (g_value_get_uint (va->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (va->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (va->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);
  g_assert_cmpuint (g_value_get_uint (va->values + 3), ==,
      TP_MEDIA_STREAM_STATE_DISCONNECTED);
  g_assert_cmpuint (g_value_get_uint (va->values + 4), ==,
      TP_MEDIA_STREAM_DIRECTION_NONE);
  g_assert_cmpuint (g_value_get_uint (va->values + 5), ==, 0);

  /* FIXME: untested things include:
   * RequestStream failing (invalid handle, invalid media type)
   * RequestStreamDirection
   * RequestStreamDirection failing (invalid direction)
   * RemoveStreams
   * RemoveStreams failing (with a contact who accepts)
   * StreamAdded being emitted correctly (part of calling RS again)
   * StreamDirectionChanged being emitted correctly (part of RSD)
   * StreamError being emitted (special contact)
   * StreamRemoved being emitted
   * StreamStateChanged being emitted (???)
   */
}

/* FIXME: add a special contact who never accepts the call, so it rings
 * forever */

/* FIXME: add a special contact who accepts the call, then terminates it */

/* FIXME: add a special contact who rejects the call with BUSY */

/* FIXME: add a special contact who refuses to have video */

/* FIXME: add a special contact who asks us for video */

/* FIXME: add a special contact whose stream errors */

static void
teardown (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  tp_cli_connection_run_disconnect (test->conn, -1, &test->error, NULL);
  test_assert_no_error (test->error);

  CLEAR_OBJECT (&test->chan);
  CLEAR_OBJECT (&test->conn);
  CLEAR_OBJECT (&test->cm);

  CLEAR_OBJECT (&test->service_cm);

  CLEAR_OBJECT (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/callable/basics", Test, NULL, setup, test_basics, teardown);

  return g_test_run ();
}
