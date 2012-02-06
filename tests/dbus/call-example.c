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

#include "config.h"

#include <telepathy-glib/telepathy-glib.h>

#include "examples/future/call-cm/cm.h"
#include "examples/future/call-cm/conn.h"
#include "examples/future/call-cm/call-channel.h"
#include "examples/future/call-cm/call-stream.h"
#include "extensions/extensions.h"

#include "tests/lib/util.h"

typedef struct
{
  GMainLoop *mainloop;
  TpDBusDaemon *dbus;
  GError *error /* statically initialized to NULL */ ;

  ExampleCallConnectionManager *service_cm;

  TpConnectionManager *cm;
  TpConnection *conn;
  TpChannel *chan;
  TpHandle self_handle;
  TpHandle peer_handle;

  GHashTable *get_all_return;

  GArray *audio_request;
  GArray *video_request;
  GArray *invalid_request;

  GArray *stream_ids;
  GArray *contacts;
  GPtrArray *get_contents_return;
  GHashTable *get_members_return;
  guint uint_return;

  gulong members_changed_detailed_id;

  FutureCallContent *added_content;
  FutureCallContent *audio_content;
  FutureCallContent *video_content;
  FutureCallStream *audio_stream;
  FutureCallStream *video_stream;
} Test;

static void
setup (Test *test,
       gconstpointer data G_GNUC_UNUSED)
{
  TpBaseConnectionManager *service_cm_as_base;
  gboolean ok;
  gchar *bus_name;
  gchar *object_path;
  GHashTable *parameters;
  guint audio = TP_MEDIA_STREAM_TYPE_AUDIO;
  guint video = TP_MEDIA_STREAM_TYPE_VIDEO;
  guint not_a_media_type = 31337;
  GQuark conn_features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->service_cm = EXAMPLE_CALL_CONNECTION_MANAGER (
      tp_tests_object_new_static_class (
        EXAMPLE_TYPE_CALL_CONNECTION_MANAGER,
        NULL));
  g_assert (test->service_cm != NULL);
  service_cm_as_base = TP_BASE_CONNECTION_MANAGER (test->service_cm);
  g_assert (service_cm_as_base != NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  test->cm = tp_connection_manager_new (test->dbus, "example_call",
      NULL, &test->error);
  g_assert (test->cm != NULL);
  tp_tests_proxy_run_until_prepared (test->cm, NULL);

  parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_hash_table_insert (parameters, "account",
      tp_g_value_slice_new_static_string ("me"));
  g_hash_table_insert (parameters, "simulation-delay",
      tp_g_value_slice_new_uint (0));

  tp_cli_connection_manager_run_request_connection (test->cm, -1,
      "example", parameters, &bus_name, &object_path, &test->error, NULL);
  g_assert_no_error (test->error);

  test->conn = tp_connection_new (test->dbus, bus_name, object_path,
      &test->error);
  g_assert_no_error (test->error);
  g_assert (test->conn != NULL);
  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);
  tp_tests_proxy_run_until_prepared (test->conn, conn_features);

  test->self_handle = tp_connection_get_self_handle (test->conn);
  g_assert (test->self_handle != 0);

  test->audio_request = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
  g_array_append_val (test->audio_request, audio);

  test->video_request = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
  g_array_append_val (test->video_request, video);

  test->invalid_request = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
  g_array_append_val (test->invalid_request, not_a_media_type);

  test->stream_ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), 2);

  g_hash_table_unref (parameters);
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

  g_assert_no_error ((GError *) error);

  test->chan = tp_channel_new_from_properties (connection, object_path,
      immutable_properties, &new_error);
  g_assert_no_error (new_error);

  test->peer_handle = tp_channel_get_handle (test->chan, NULL);

  g_main_loop_quit (test->mainloop);
}

static void
channel_ready_cb (TpChannel *channel G_GNUC_UNUSED,
                  const GError *error,
                  gpointer user_data)
{
  Test *test = user_data;

  g_assert_no_error ((GError *) error);
  g_main_loop_quit (test->mainloop);
}

static void
added_content_cb (TpChannel *chan G_GNUC_UNUSED,
    const gchar *object_path,
    const GError *error,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  tp_clear_object (&test->added_content);

  if (error != NULL)
    {
      test->error = g_error_copy (error);
    }
  else
    {
      test->added_content = future_call_content_new (test->chan, object_path,
          NULL);
      g_assert (test->added_content != NULL);
    }

  g_main_loop_quit (test->mainloop);
}

static void
got_all_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  g_assert_no_error ((GError *) error);

  tp_clear_pointer (&test->get_all_return, g_hash_table_unref);
  test->get_all_return = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) tp_g_value_slice_free);
  tp_g_hash_table_update (test->get_all_return, properties,
      (GBoxedCopyFunc) g_strdup, (GBoxedCopyFunc) tp_g_value_slice_dup);

  g_main_loop_quit (test->mainloop);
}

static void
got_contents_cb (TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  g_assert_no_error ((GError *) error);

  tp_clear_boxed (TP_ARRAY_TYPE_OBJECT_PATH_LIST, &test->get_contents_return);
  g_assert (G_VALUE_HOLDS (value, TP_ARRAY_TYPE_OBJECT_PATH_LIST));
  test->get_contents_return = g_value_dup_boxed (value);

  g_main_loop_quit (test->mainloop);
}

static void
got_members_cb (TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  tp_clear_pointer (&test->get_members_return, g_hash_table_unref);

  if (test->error != NULL)
    g_clear_error (&test->error);

  g_assert_no_error ((GError *) error);

  g_assert (G_VALUE_HOLDS (value, FUTURE_HASH_TYPE_CONTACT_SENDING_STATE_MAP));
  test->get_members_return = g_value_dup_boxed (value);

  g_main_loop_quit (test->mainloop);
}

static void
got_uint_cb (TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  if (test->error != NULL)
    g_clear_error (&test->error);

  g_assert_no_error ((GError *) error);

  g_assert (G_VALUE_HOLDS (value, G_TYPE_UINT));
  test->uint_return = g_value_get_uint (value);

  g_main_loop_quit (test->mainloop);
}

static void
void_cb (TpChannel *chan G_GNUC_UNUSED,
         const GError *error,
         gpointer user_data,
         GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  if (error != NULL)
    {
      test->error = g_error_copy (error);
    }

  g_main_loop_quit (test->mainloop);
}

static void
test_connect_channel_signals (Test *test)
{
}

static void
outgoing_call (Test *test,
               const gchar *id,
               gboolean initial_audio,
               gboolean initial_video)
{
  GHashTable *request = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE,
          G_TYPE_STRING, FUTURE_IFACE_CHANNEL_TYPE_CALL,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT, TP_HANDLE_TYPE_CONTACT,
      TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, id,
      FUTURE_PROP_CHANNEL_TYPE_CALL_INITIAL_AUDIO,
          G_TYPE_BOOLEAN, initial_audio,
      FUTURE_PROP_CHANNEL_TYPE_CALL_INITIAL_VIDEO,
          G_TYPE_BOOLEAN, initial_video,
      NULL);

  tp_cli_connection_interface_requests_call_create_channel (test->conn, -1,
      request, channel_created_cb, test, NULL, NULL);
  g_hash_table_unref (request);
  request = NULL;
  g_main_loop_run (test->mainloop);

  /* Do this before waiting for it to become ready - we knew its channel type
   * and interfaces anyway */
  test_connect_channel_signals (test);

  tp_channel_call_when_ready (test->chan, channel_ready_cb, test);
  g_main_loop_run (test->mainloop);
}

static void
assert_call_properties (GHashTable *get_all_return,
    FutureCallState call_state,
    TpHandle actor,
    FutureCallStateChangeReason reason,
    const gchar *dbus_reason,
    gboolean check_call_flags, FutureCallFlags call_flags,
    gboolean check_initials, gboolean initial_audio, gboolean initial_video)
{
  gboolean valid;
  GValueArray *state_reason;

  g_assert_cmpuint (tp_asv_get_uint32 (get_all_return, "CallState",
        &valid), ==, call_state);
  g_assert (valid);
  state_reason = tp_asv_get_boxed (get_all_return, "CallStateReason",
      FUTURE_STRUCT_TYPE_CALL_STATE_REASON);
  g_assert (state_reason != NULL);
  g_assert_cmpuint (g_value_get_uint (state_reason->values + 0), ==,
      actor);
  g_assert_cmpuint (g_value_get_uint (state_reason->values + 1), ==,
      reason);
  g_assert_cmpstr (g_value_get_string (state_reason->values + 2), ==,
      dbus_reason);

  /* Hard-coded properties */
  g_assert_cmpint (tp_asv_get_boolean (get_all_return,
        "HardwareStreaming", &valid), ==, TRUE);
  g_assert (valid);
  g_assert_cmpint (tp_asv_get_boolean (get_all_return,
        "MutableContents", &valid), ==, TRUE);
  g_assert (valid);
  g_assert_cmpuint (tp_asv_get_uint32 (get_all_return,
        "InitialTransport", &valid), ==, FUTURE_STREAM_TRANSPORT_TYPE_UNKNOWN);
  g_assert (valid);

  if (check_call_flags)
    {
      g_assert_cmpuint (tp_asv_get_uint32 (get_all_return,
            "CallFlags", &valid), ==, 0);
      g_assert (valid);
    }

  if (check_initials)
    {
      g_assert_cmpint (tp_asv_get_boolean (get_all_return,
            "InitialAudio", &valid), ==, initial_audio);
      g_assert (valid);

      g_assert_cmpint (tp_asv_get_boolean (get_all_return,
            "InitialVideo", &valid), ==, initial_video);
      g_assert (valid);
    }

  /* FIXME: CallStateDetails */
}

static void
assert_content_properties (GHashTable *get_all_return,
    TpMediaStreamType type,
    FutureCallContentDisposition disposition)
{
  gboolean valid;

  g_assert_cmpstr (tp_asv_get_string (get_all_return, "Name"), !=, NULL);
  g_assert_cmpuint (tp_asv_get_uint32 (get_all_return, "Type", &valid),
      ==, type);
  g_assert_cmpint (valid, ==, TRUE);
  g_assert_cmpint (valid, ==, TRUE);
  g_assert_cmpuint (tp_asv_get_uint32 (get_all_return, "Disposition",
        &valid), ==, disposition);
  g_assert_cmpint (valid, ==, TRUE);
}

static void
loop_until_ended (Test *test)
{
  while (1)
    {
      tp_cli_dbus_properties_call_get_all (test->chan, -1,
          FUTURE_IFACE_CHANNEL_TYPE_CALL, got_all_cb, test, NULL, NULL);
      g_main_loop_run (test->mainloop);
      g_assert_no_error (test->error);

      if (tp_asv_get_uint32 (test->get_all_return, "CallState",
            NULL) == FUTURE_CALL_STATE_ENDED)
        return;
    }
}

static void
loop_until_answered (Test *test)
{
  while (1)
    {
      tp_cli_dbus_properties_call_get_all (test->chan, -1,
          FUTURE_IFACE_CHANNEL_TYPE_CALL, got_all_cb, test, NULL, NULL);
      g_main_loop_run (test->mainloop);
      g_assert_no_error (test->error);

      if (tp_asv_get_uint32 (test->get_all_return, "CallState",
            NULL) != FUTURE_CALL_STATE_RINGING)
        return;
    }
}

static void
assert_ended_and_run_close (Test *test,
    TpHandle expected_actor,
    FutureCallStateChangeReason expected_reason,
    const gchar *expected_error)
{
  /* In response to whatever we just did, the call ends... */
  tp_cli_dbus_properties_call_get_all (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, got_all_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  assert_call_properties (test->get_all_return,
      FUTURE_CALL_STATE_ENDED,
      expected_actor,
      expected_reason,
      expected_error,
      FALSE, 0, /* ignore call flags */
      FALSE, FALSE, FALSE); /* ignore initial audio/video */

  /* ... which means there are no contents ... */
  tp_cli_dbus_properties_call_get (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, "Contents",
      got_contents_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  g_assert_cmpuint (test->get_contents_return->len, ==, 0);

  /* ... but the channel doesn't close */
  tp_tests_proxy_run_until_dbus_queue_processed (test->conn);
  g_assert (tp_proxy_get_invalidated (test->chan) == NULL);

  /* When we call Close it finally closes */
  tp_cli_channel_call_close (test->chan, -1, void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  tp_tests_proxy_run_until_dbus_queue_processed (test->conn);
  g_assert (tp_proxy_get_invalidated (test->chan) != NULL);
}

static void
test_basics (Test *test,
             gconstpointer data G_GNUC_UNUSED)
{
  const GPtrArray *stream_paths;
  gpointer v;

  outgoing_call (test, "basic-test", TRUE, FALSE);

  /* Get initial state */
  tp_cli_dbus_properties_call_get_all (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, got_all_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  assert_call_properties (test->get_all_return,
      FUTURE_CALL_STATE_PENDING_INITIATOR, 0,
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
      TRUE, 0,              /* call flags */
      TRUE, TRUE, FALSE);  /* initial audio/video must be what we said */

  /* We have one audio content but it's not active just yet */

  tp_cli_dbus_properties_call_get (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, "Contents",
      got_contents_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (test->get_contents_return->len, ==, 1);

  g_assert (test->audio_content == NULL);
  test->audio_content = future_call_content_new (test->chan,
      g_ptr_array_index (test->get_contents_return, 0), NULL);
  g_assert (test->audio_content != NULL);

  tp_cli_dbus_properties_call_get_all (test->audio_content, -1,
      FUTURE_IFACE_CALL_CONTENT, got_all_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  assert_content_properties (test->get_all_return,
      TP_MEDIA_STREAM_TYPE_AUDIO,
      FUTURE_CALL_CONTENT_DISPOSITION_INITIAL);

  stream_paths = tp_asv_get_boxed (test->get_all_return, "Streams",
          TP_ARRAY_TYPE_OBJECT_PATH_LIST);
  g_assert (stream_paths != NULL);
  g_assert_cmpuint (stream_paths->len, ==, 1);

  g_assert (test->audio_stream == NULL);
  test->audio_stream = future_call_stream_new (test->chan,
      g_ptr_array_index (stream_paths, 0), NULL);
  g_assert (test->audio_stream != NULL);

  tp_cli_dbus_properties_call_get (test->audio_stream, -1,
      FUTURE_IFACE_CALL_STREAM, "RemoteMembers",
      got_members_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (g_hash_table_size (test->get_members_return), ==, 1);
  g_assert (!g_hash_table_lookup_extended (test->get_members_return,
        GUINT_TO_POINTER (0), NULL, NULL));
  g_assert (!g_hash_table_lookup_extended (test->get_members_return,
        GUINT_TO_POINTER (test->self_handle), NULL, NULL));
  g_assert (g_hash_table_lookup_extended (test->get_members_return,
        GUINT_TO_POINTER (tp_channel_get_handle (test->chan, NULL)),
        NULL, &v));
  g_assert_cmpuint (GPOINTER_TO_UINT (v), ==,
      FUTURE_SENDING_STATE_PENDING_SEND);

  tp_cli_dbus_properties_call_get (test->audio_stream, -1,
      FUTURE_IFACE_CALL_STREAM, "LocalSendingState",
      got_uint_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  g_assert_cmpuint (test->uint_return, ==, FUTURE_SENDING_STATE_SENDING);

  /* OK, that looks good. Actually make the call */
  future_cli_channel_type_call_call_accept (test->chan, -1, void_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Calling Accept again makes no sense, but mustn't crash */
  future_cli_channel_type_call_call_accept (test->chan, -1, void_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE);
  g_clear_error (&test->error);

  /* Wait for the remote contact to answer, if they haven't already */

  loop_until_answered (test);

  /* Calling Accept again makes no sense, but mustn't crash */
  future_cli_channel_type_call_call_accept (test->chan, -1, void_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE);
  g_clear_error (&test->error);

  /* Check the call state */

  tp_cli_dbus_properties_call_get_all (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, got_all_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  assert_call_properties (test->get_all_return,
      FUTURE_CALL_STATE_ACCEPTED, tp_channel_get_handle (test->chan, NULL),
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
      TRUE, 0,              /* call flags */
      FALSE, FALSE, FALSE); /* don't care about initial audio/video */

  /* There's still one content */
  tp_clear_boxed (TP_ARRAY_TYPE_OBJECT_PATH_LIST, &test->get_contents_return);
  test->get_contents_return = g_boxed_copy (TP_ARRAY_TYPE_OBJECT_PATH_LIST,
      tp_asv_get_boxed (test->get_all_return,
        "Contents", TP_ARRAY_TYPE_OBJECT_PATH_LIST));
  g_assert_cmpuint (test->get_contents_return->len, ==, 1);
  g_assert_cmpstr (g_ptr_array_index (test->get_contents_return, 0),
      ==, tp_proxy_get_object_path (test->audio_content));

  /* Other contact is sending now */
  tp_clear_pointer (&test->get_members_return, g_hash_table_unref);
  tp_cli_dbus_properties_call_get (test->audio_stream, -1,
      FUTURE_IFACE_CALL_STREAM, "RemoteMembers", got_members_cb, test,
      NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (g_hash_table_size (test->get_members_return), ==, 1);
  g_assert (!g_hash_table_lookup_extended (test->get_members_return,
        GUINT_TO_POINTER (0), NULL, NULL));
  g_assert (!g_hash_table_lookup_extended (test->get_members_return,
        GUINT_TO_POINTER (test->self_handle), NULL, NULL));
  g_assert (g_hash_table_lookup_extended (test->get_members_return,
        GUINT_TO_POINTER (tp_channel_get_handle (test->chan, NULL)),
        NULL, &v));
  g_assert_cmpuint (GPOINTER_TO_UINT (v), ==, FUTURE_SENDING_STATE_SENDING);

  tp_cli_dbus_properties_call_get (test->audio_stream, -1,
      FUTURE_IFACE_CALL_STREAM, "LocalSendingState",
      got_uint_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  g_assert_cmpuint (test->uint_return, ==, FUTURE_SENDING_STATE_SENDING);

  /* AddContent with bad content-type must fail */

  future_cli_channel_type_call_call_add_content (test->chan, -1,
      "", 31337, added_content_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert (test->error != NULL);
  g_clear_error (&test->error);

  /* AddContent again, to add a video stream */

  future_cli_channel_type_call_call_add_content (test->chan, -1,
      "", TP_MEDIA_STREAM_TYPE_VIDEO, added_content_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->added_content != NULL);
  tp_clear_object (&test->video_content);
  test->video_content = g_object_ref (test->added_content);

  /* There are two Contents, because now we have the video content too */

  tp_cli_dbus_properties_call_get (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, "Contents",
      got_contents_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (test->get_contents_return->len, ==, 2);

  /* they could be either way round */
  if (!tp_strdiff (g_ptr_array_index (test->get_contents_return, 0),
        tp_proxy_get_object_path (test->audio_content)))
    {
      g_assert_cmpstr (g_ptr_array_index (test->get_contents_return, 1),
          ==, tp_proxy_get_object_path (test->video_content));
    }
  else
    {
      g_assert_cmpstr (g_ptr_array_index (test->get_contents_return, 0),
          ==, tp_proxy_get_object_path (test->video_content));
      g_assert_cmpstr (g_ptr_array_index (test->get_contents_return, 1),
          ==, tp_proxy_get_object_path (test->audio_content));
    }

  tp_cli_dbus_properties_call_get_all (test->video_content, -1,
      FUTURE_IFACE_CALL_CONTENT, got_all_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  assert_content_properties (test->get_all_return,
      TP_MEDIA_STREAM_TYPE_VIDEO,
      FUTURE_CALL_CONTENT_DISPOSITION_NONE);

  stream_paths = tp_asv_get_boxed (test->get_all_return, "Streams",
          TP_ARRAY_TYPE_OBJECT_PATH_LIST);
  g_assert (stream_paths != NULL);
  g_assert_cmpuint (stream_paths->len, ==, 1);

  g_assert (test->video_stream == NULL);
  test->video_stream = future_call_stream_new (test->chan,
      g_ptr_array_index (stream_paths, 0), NULL);
  g_assert (test->video_stream != NULL);

  tp_cli_dbus_properties_call_get (test->audio_stream, -1,
      FUTURE_IFACE_CALL_STREAM, "LocalSendingState",
      got_uint_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  g_assert_cmpuint (test->uint_return, ==, FUTURE_SENDING_STATE_SENDING);

  tp_cli_dbus_properties_call_get (test->video_stream, -1,
      FUTURE_IFACE_CALL_STREAM, "RemoteMembers", got_members_cb, test,
      NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (g_hash_table_size (test->get_members_return), ==, 1);
  g_assert (!g_hash_table_lookup_extended (test->get_members_return,
        GUINT_TO_POINTER (0), NULL, NULL));
  g_assert (!g_hash_table_lookup_extended (test->get_members_return,
        GUINT_TO_POINTER (test->self_handle), NULL, NULL));
  g_assert (g_hash_table_lookup_extended (test->get_members_return,
        GUINT_TO_POINTER (tp_channel_get_handle (test->chan, NULL)),
        NULL, &v));

  /* After a moment, the video stream becomes connected, and the remote user
   * accepts our proposed direction change. These might happen in either
   * order, at least in this implementation. */

  if (GPOINTER_TO_UINT (v) != FUTURE_SENDING_STATE_SENDING)
    g_assert_cmpuint (GPOINTER_TO_UINT (v), ==,
        FUTURE_SENDING_STATE_PENDING_SEND);

#if 0
  /* FIXME: Call has no equivalent of RemoveStreams yet, afaics... */

  /* RemoveStreams with a bad stream ID must fail */

  g_array_set_size (test->stream_ids, 0);
  g_array_append_val (test->stream_ids, not_a_stream_id);
  tp_cli_channel_type_streamed_media_call_remove_streams (test->chan, -1,
      test->stream_ids,
      void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert (test->error != NULL);
  g_clear_error (&test->error);

  /* Drop the video stream with RemoveStreams */

  g_array_set_size (test->stream_ids, 0);
  g_array_append_val (test->stream_ids, video_stream_id);
  tp_cli_channel_type_streamed_media_call_remove_streams (test->chan, -1,
      test->stream_ids,
      void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Get contents again: now there's only the audio */

  tp_cli_dbus_properties_call_get (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, "Contents",
      got_contents_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (test->get_contents_return->len, ==, 1);
  g_assert_cmpstr (g_ptr_array_index (test->get_contents_return, 0), ==,
      tp_proxy_get_object_path (test->audio_content));
#endif

  /* Hang up the call in the recommended way */

  future_cli_channel_type_call_call_hangup (test->chan,
      -1, FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "", "",
      void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  assert_ended_and_run_close (test, test->self_handle,
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
      "");

  /* FIXME: untested things include:
   *
   * RequestStreamDirection
   * StreamDirectionChanged being emitted correctly (part of RSD)
   * RequestStreamDirection failing (invalid direction, stream ID)
   */
}

static void
test_no_answer (Test *test,
                gconstpointer data G_GNUC_UNUSED)
{
  /* This identifier contains the magic string (no answer), which means the
   * example will never answer. */
  outgoing_call (test, "smcv (no answer)", TRUE, FALSE);

  future_cli_channel_type_call_call_accept (test->chan, -1, void_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* After the initial flurry of D-Bus messages, smcv still hasn't answered */
  tp_tests_proxy_run_until_dbus_queue_processed (test->conn);

  tp_cli_dbus_properties_call_get_all (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, got_all_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  assert_call_properties (test->get_all_return,
      FUTURE_CALL_STATE_RINGING, test->self_handle,
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
      TRUE, 0,              /* call flags */
      TRUE, TRUE, FALSE);  /* initial audio/video must be TRUE, FALSE */

  /* assume we're never going to get an answer, and hang up */
  future_cli_channel_type_call_call_hangup (test->chan,
      -1, FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "", "",
      void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  assert_ended_and_run_close (test, test->self_handle,
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
      "");
}

static void
test_busy (Test *test,
           gconstpointer data G_GNUC_UNUSED)
{
  /* This identifier contains the magic string (busy), which means the example
   * will simulate rejection of the call as busy rather than accepting it. */
  outgoing_call (test, "Robot101 (busy)", TRUE, FALSE);

  future_cli_channel_type_call_call_accept (test->chan, -1, void_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Wait for the remote contact to end the call as busy */
  loop_until_ended (test);
  assert_ended_and_run_close (test, tp_channel_get_handle (test->chan, NULL),
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
      TP_ERROR_STR_BUSY);
}

static void
test_terminated_by_peer (Test *test,
                         gconstpointer data G_GNUC_UNUSED)
{
  /* This contact contains the magic string "(terminate)", meaning the example
   * simulates answering the call but then terminating it */
  outgoing_call (test, "The Governator (terminate)", TRUE, TRUE);

  future_cli_channel_type_call_call_accept (test->chan, -1, void_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Wait for the remote contact to answer, if they haven't already */

  loop_until_answered (test);

  /* After that, the remote contact immediately ends the call */
  loop_until_ended (test);
  assert_ended_and_run_close (test, tp_channel_get_handle (test->chan, NULL),
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
      "");
}

static void
test_terminate_via_close (Test *test,
                          gconstpointer data G_GNUC_UNUSED)
{
  outgoing_call (test, "basic-test", FALSE, TRUE);

  future_cli_channel_type_call_call_accept (test->chan, -1, void_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Wait for the remote contact to answer, if they haven't already */

  loop_until_answered (test);

  tp_cli_dbus_properties_call_get_all (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, got_all_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  assert_call_properties (test->get_all_return,
      FUTURE_CALL_STATE_ACCEPTED, test->peer_handle,
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
      TRUE, 0,              /* call flags */
      TRUE, FALSE, TRUE);  /* initial audio/video must be FALSE, TRUE */

  /* Terminate the call unceremoniously, by calling Close. This is not a
   * graceful hangup; rather, it's what the ChannelDispatcher would do to
   * signal a client crash, undispatchability, or whatever */

  tp_cli_channel_call_close (test->chan, -1, void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* In response to termination, the channel does genuinely close */
  tp_tests_proxy_run_until_dbus_queue_processed (test->conn);
  g_assert (tp_proxy_get_invalidated (test->chan) != NULL);

  /* FIXME: when we hook up signals, check for expected call state
   * transition before invalidation */
}

/* FIXME: try removing the last stream. In StreamedMedia that terminated the
 * call, but in Call it's meant to just fail */

/* FIXME: add a special contact who refuses to have video */

/* FIXME: add a special contact who asks us for video */

/* FIXME: add a special contact whose stream errors */

static void
expect_incoming_call_cb (TpConnection *conn,
                         const GPtrArray *channels,
                         gpointer user_data,
                         GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  guint i;

  for (i = 0; i < channels->len; i++)
    {
      GValueArray *va = g_ptr_array_index (channels, i);
      const gchar *object_path = g_value_get_boxed (va->values + 0);
      GHashTable *properties = g_value_get_boxed (va->values + 1);
      const gchar *channel_type;

      channel_type = tp_asv_get_string (properties,
          TP_PROP_CHANNEL_CHANNEL_TYPE);
      if (tp_strdiff (channel_type, FUTURE_IFACE_CHANNEL_TYPE_CALL))
        {
          /* don't care about this channel */
          continue;
        }

      g_assert_cmpuint (tp_asv_get_uint32 (properties,
            TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, NULL),
          ==, TP_HANDLE_TYPE_CONTACT);
      g_assert_cmpint (tp_asv_get_boolean (properties,
            TP_PROP_CHANNEL_REQUESTED, NULL), ==, FALSE);

      /* we only expect to receive one call */
      g_assert (test->chan == NULL);

      /* save the channel */
      test->chan = tp_channel_new_from_properties (conn, object_path,
          properties, &test->error);
      g_assert_no_error (test->error);
    }
}

/* In this example connection manager, every time the presence status changes
 * to available or the message changes, an incoming call is simulated. */
static void
trigger_incoming_call (Test *test,
                       const gchar *message,
                       const gchar *expected_caller)
{
  TpProxySignalConnection *new_channels_sig;

  tp_cli_connection_interface_simple_presence_run_set_presence (test->conn, -1,
      "away", "preparing for a test", &test->error, NULL);
  g_assert_no_error (test->error);

  new_channels_sig =
    tp_cli_connection_interface_requests_connect_to_new_channels (test->conn,
        expect_incoming_call_cb, test, NULL, NULL, &test->error);
  g_assert_no_error (test->error);

  tp_cli_connection_interface_simple_presence_run_set_presence (test->conn, -1,
      "available", message, &test->error, NULL);
  g_assert_no_error (test->error);

  /* wait for the call to happen if it hasn't already */
  while (test->chan == NULL)
    {
      g_main_context_iteration (NULL, TRUE);
    }

  g_assert_cmpstr (tp_channel_get_identifier (test->chan), ==,
      expected_caller);
  test->peer_handle = tp_channel_get_handle (test->chan, NULL);

  tp_proxy_signal_connection_disconnect (new_channels_sig);

  tp_channel_call_when_ready (test->chan, channel_ready_cb, test);
  g_main_loop_run (test->mainloop);
  test_connect_channel_signals (test);
}

static void
test_incoming (Test *test,
               gconstpointer data G_GNUC_UNUSED)
{
  trigger_incoming_call (test, "call me?", "caller");

  /* ring, ring! */
  tp_cli_dbus_properties_call_get_all (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, got_all_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  assert_call_properties (test->get_all_return,
      FUTURE_CALL_STATE_RINGING, test->peer_handle,
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
      TRUE, 0,              /* call flags */
      TRUE, TRUE, FALSE);  /* initial audio/video must be TRUE, FALSE */

  /* Get Contents: we have an audio content (FIXME: assert that) */

  tp_cli_dbus_properties_call_get (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, "Contents",
      got_contents_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (test->get_contents_return->len, ==, 1);

  /* FIXME: assert about the properties of the content and the stream */

  /* Accept the call */
  future_cli_channel_type_call_call_accept (test->chan, -1, void_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  tp_cli_dbus_properties_call_get_all (test->chan, -1,
      FUTURE_IFACE_CHANNEL_TYPE_CALL, got_all_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  assert_call_properties (test->get_all_return,
      FUTURE_CALL_STATE_ACCEPTED, test->self_handle,
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
      TRUE, 0,              /* call flags */
      TRUE, TRUE, FALSE);  /* initial audio/video are still TRUE, FALSE */

  /* FIXME: check for stream directionality changes */

  /* Hang up the call */
  future_cli_channel_type_call_call_hangup (test->chan,
      -1, FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "", "",
      void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  assert_ended_and_run_close (test, test->self_handle,
      FUTURE_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "");
}

static void
teardown (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  tp_tests_connection_assert_disconnect_succeeds (test->conn);

  if (test->members_changed_detailed_id != 0)
    {
      g_signal_handler_disconnect (test->chan,
          test->members_changed_detailed_id);
    }

  g_array_unref (test->audio_request);
  g_array_unref (test->video_request);
  g_array_unref (test->invalid_request);
  g_array_unref (test->stream_ids);
  tp_clear_pointer (&test->get_all_return, g_hash_table_unref);

  tp_clear_boxed (TP_ARRAY_TYPE_OBJECT_PATH_LIST, &test->get_contents_return);
  tp_clear_pointer (&test->get_members_return, g_hash_table_unref);

  tp_clear_object (&test->audio_stream);
  tp_clear_object (&test->video_stream);
  tp_clear_object (&test->added_content);
  tp_clear_object (&test->audio_content);
  tp_clear_object (&test->video_content);
  tp_clear_object (&test->chan);
  tp_clear_object (&test->conn);
  tp_clear_object (&test->cm);

  tp_clear_object (&test->service_cm);

  /* make sure any pending things have happened */
  tp_tests_proxy_run_until_dbus_queue_processed (test->dbus);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");
  g_set_prgname ("call-example");

  future_cli_init ();

  g_test_add ("/call/basics", Test, NULL, setup, test_basics, teardown);
  g_test_add ("/call/busy", Test, NULL, setup, test_busy, teardown);
  g_test_add ("/call/no-answer", Test, NULL, setup, test_no_answer,
      teardown);
  g_test_add ("/call/terminated-by-peer", Test, NULL, setup,
      test_terminated_by_peer, teardown);
  g_test_add ("/call/terminate-via-close", Test, NULL, setup,
      test_terminate_via_close, teardown);
  g_test_add ("/call/incoming", Test, NULL, setup, test_incoming,
      teardown);

  return g_test_run ();
}
