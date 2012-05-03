/* Tests for TpCallChannel, TpCallContent and TpCallStream
 *
 * Copyright © 2009-2011 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/proxy-subclass.h>

#include "examples/cm/call/cm.h"
#include "examples/cm/call/conn.h"
#include "examples/cm/call/call-channel.h"
#include "examples/cm/call/call-stream.h"

#include "tests/lib/util.h"

typedef struct
{
  GMainLoop *mainloop;
  TpDBusDaemon *dbus;
  GError *error /* statically initialized to NULL */ ;
  guint wait_count;

  ExampleCallConnectionManager *service_cm;

  TpSimpleClientFactory *factory;
  TpConnectionManager *cm;
  TpConnection *conn;
  TpChannel *chan;
  TpCallChannel *call_chan;
  TpHandle self_handle;
  TpHandle peer_handle;

  GArray *audio_request;
  GArray *video_request;
  GArray *invalid_request;

  GArray *stream_ids;
  GArray *contacts;

  TpCallContent *added_content;
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

  test->factory = (TpSimpleClientFactory *)
      tp_automatic_client_factory_new (test->dbus);
  tp_simple_client_factory_add_channel_features_varargs (test->factory,
      TP_CHANNEL_FEATURE_CONTACTS,
      0);

  test->conn = tp_simple_client_factory_ensure_connection (test->factory,
      object_path, NULL, &test->error);
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

  test->chan = tp_simple_client_factory_ensure_channel (test->factory,
      connection, object_path, immutable_properties, &new_error);
  g_assert_no_error (new_error);

  g_assert (TP_IS_CALL_CHANNEL (test->chan));
  test->call_chan = (TpCallChannel *) test->chan;

  test->peer_handle = tp_channel_get_handle (test->chan, NULL);

  g_main_loop_quit (test->mainloop);
}

static void
outgoing_call (Test *test,
               const gchar *id,
               gboolean initial_audio,
               gboolean initial_video)
{
  GHashTable *request = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE,
          G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_CALL,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT, TP_HANDLE_TYPE_CONTACT,
      TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, id,
      TP_PROP_CHANNEL_TYPE_CALL_INITIAL_AUDIO,
          G_TYPE_BOOLEAN, initial_audio,
      TP_PROP_CHANNEL_TYPE_CALL_INITIAL_VIDEO,
          G_TYPE_BOOLEAN, initial_video,
      NULL);

  tp_cli_connection_interface_requests_call_create_channel (test->conn, -1,
      request, channel_created_cb, test, NULL, NULL);
  g_hash_table_unref (request);
  request = NULL;
  g_main_loop_run (test->mainloop);

  tp_tests_proxy_run_until_prepared (test->chan, NULL);
}

static void
assert_call_properties (TpCallChannel *channel,
    TpCallState call_state,
    TpHandle actor,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    gboolean check_call_flags, TpCallFlags call_flags,
    gboolean check_initials, gboolean initial_audio, gboolean initial_video)
{
  TpCallState state;
  TpCallFlags flags;
  GHashTable *details;
  TpCallStateReason *r;

  state = tp_call_channel_get_state (channel, &flags, &details, &r);

  /* FIXME: details */
  g_assert_cmpuint (state, ==, call_state);
  g_assert_cmpuint (r->actor, ==, actor);
  g_assert_cmpuint (r->reason, ==, reason);
  g_assert_cmpstr (r->dbus_reason, ==, dbus_reason);
  if (check_call_flags)
    g_assert_cmpuint (flags, ==, call_flags);

  /* Hard-coded properties */
  g_assert_cmpint (tp_call_channel_has_hardware_streaming (channel), ==, FALSE);
  g_assert_cmpint (tp_call_channel_has_mutable_contents (channel), ==, TRUE);

  if (check_initials)
    {
      const gchar *initial_audio_name;
      const gchar *initial_video_name;

      g_assert_cmpint (tp_call_channel_has_initial_audio (channel,
          &initial_audio_name), ==, initial_audio);
      g_assert_cmpint (tp_call_channel_has_initial_video (channel,
          &initial_video_name), ==, initial_video);
      g_assert_cmpstr (initial_audio_name, ==, initial_audio ? "audio" : NULL);
      g_assert_cmpstr (initial_video_name, ==, initial_video ? "video" : NULL);
    }
}

static void
assert_content_properties (TpCallContent *content,
    TpMediaStreamType type,
    TpCallContentDisposition disposition)
{
  g_assert_cmpstr (tp_call_content_get_name (content), !=, NULL);
  g_assert_cmpuint (tp_call_content_get_media_type (content), ==, type);
  g_assert_cmpuint (tp_call_content_get_disposition (content), ==, disposition);
}

static void
close_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  g_clear_error (&test->error);

  tp_channel_close_finish (test->chan, result, &test->error);
  g_main_loop_quit (test->mainloop);
}

static void
assert_ended_and_run_close (Test *test,
    TpHandle expected_actor,
    TpCallStateChangeReason expected_reason,
    const gchar *expected_error)
{
  GPtrArray *contents;

  tp_tests_proxy_run_until_dbus_queue_processed (test->conn);

  /* In response to whatever we just did, the call ends... */
  assert_call_properties (test->call_chan,
      TP_CALL_STATE_ENDED,
      expected_actor,
      expected_reason,
      expected_error,
      FALSE, 0, /* ignore call flags */
      FALSE, FALSE, FALSE); /* ignore initial audio/video */

  /* ... which means there are no contents ... */
  contents = tp_call_channel_get_contents (test->call_chan);
  g_assert_cmpuint (contents->len, ==, 0);

  /* ... but the channel doesn't close */
  g_assert (tp_proxy_get_invalidated (test->chan) == NULL);

  /* When we call Close it finally closes */
  tp_channel_close_async (test->chan, close_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  tp_tests_proxy_run_until_dbus_queue_processed (test->conn);
  g_assert (tp_proxy_get_invalidated (test->chan) != NULL);
}

static void
run_until_answered_cb (TpCallChannel *channel,
    TpCallState state,
    TpCallFlags flags,
    TpCallStateReason *reason,
    GHashTable *details,
    Test *test)
{
  if (state != TP_CALL_STATE_INITIALISED)
    g_main_loop_quit (test->mainloop);
}

static void
run_until_answered (Test *test)
{
  TpCallState state;
  guint id;

  state = tp_call_channel_get_state (test->call_chan, NULL, NULL, NULL);
  if (state != TP_CALL_STATE_INITIALISED)
    return;

  id = g_signal_connect (test->call_chan, "state-changed",
      G_CALLBACK (run_until_answered_cb), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->call_chan, id);
}

static void
run_until_ended_cb (TpCallChannel *channel,
    TpCallState state,
    TpCallFlags flags,
    TpCallStateReason *reason,
    GHashTable *details,
    Test *test)
{
  if (state == TP_CALL_STATE_ENDED)
    g_main_loop_quit (test->mainloop);
}

static void
run_until_ended (Test *test)
{
  TpCallState state;
  guint id;

  state = tp_call_channel_get_state (test->call_chan, NULL, NULL, NULL);
  if (state == TP_CALL_STATE_ENDED)
    return;

  id = g_signal_connect (test->call_chan, "state-changed",
      G_CALLBACK (run_until_ended_cb), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->call_chan, id);
}

static void
run_until_active_cb (TpCallChannel *channel,
    TpCallState state,
    TpCallFlags flags,
    TpCallStateReason *reason,
    GHashTable *details,
    Test *test)
{
  if (state == TP_CALL_STATE_ACTIVE)
    g_main_loop_quit (test->mainloop);
}

static void
run_until_active_get_all_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GPtrArray *endpoints;
  guint i;

  g_assert_no_error (error);

  tp_asv_dump (properties);

  endpoints = tp_asv_get_boxed (properties, "Endpoints",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);
  g_assert (endpoints != NULL);
  g_assert (endpoints->len > 0);

  for (i = 0; i < endpoints->len; i++)
    {
      const gchar *object_path = g_ptr_array_index (endpoints, i);
      TpProxy *endpoint;

      endpoint = g_object_new (TP_TYPE_PROXY,
          "dbus-daemon", tp_proxy_get_dbus_daemon (proxy),
          "bus-name", tp_proxy_get_bus_name (proxy),
          "object-path", object_path,
          NULL);
      tp_proxy_add_interface_by_id (endpoint,
          TP_IFACE_QUARK_CALL_STREAM_ENDPOINT);

      tp_cli_call_stream_endpoint_call_set_endpoint_state (endpoint,
          -1, TP_STREAM_COMPONENT_DATA,
          TP_STREAM_ENDPOINT_STATE_FULLY_CONNECTED,
          NULL, NULL, NULL, NULL);

      g_object_unref (endpoint);
    }
}

static void
run_until_active_stream_prepared_cb (GObject *stream,
    GAsyncResult *res,
    gpointer test)
{
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (stream, res, &error))
    {
      g_error ("error %s", error->message);
    }

  g_assert (tp_proxy_has_interface_by_id (stream,
          TP_IFACE_QUARK_CALL_STREAM_INTERFACE_MEDIA));

  tp_cli_dbus_properties_call_get_all (stream, -1,
      TP_IFACE_CALL_STREAM_INTERFACE_MEDIA,
      run_until_active_get_all_cb, test, NULL,
      NULL);
}


static void
run_until_active (Test *test)
{
  GPtrArray *contents;
  guint i, j;
  guint id;

  if (tp_call_channel_get_state (test->call_chan, NULL, NULL, NULL) ==
          TP_CALL_STATE_ACTIVE)
    return;

  g_assert (tp_call_channel_get_state (test->call_chan, NULL, NULL, NULL) ==
      TP_CALL_STATE_ACCEPTED);

  contents = tp_call_channel_get_contents (test->call_chan);
  for (i = 0; i < contents->len; i++)
    {
      TpCallContent *content = g_ptr_array_index (contents, i);
      GPtrArray *streams;

      streams = tp_call_content_get_streams (content);
      for (j = 0; j < streams->len; j++)
        {
          TpCallStream *stream = g_ptr_array_index (streams, j);

          tp_proxy_prepare_async (stream, NULL,
              run_until_active_stream_prepared_cb, test);
        }
    }


  id = g_signal_connect (test->call_chan, "state-changed",
      G_CALLBACK (run_until_active_cb), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->call_chan, id);
}

static void
accept_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  g_clear_error (&test->error);

  tp_call_channel_accept_finish (test->call_chan, result, &test->error);
  g_main_loop_quit (test->mainloop);
}

static void
run_until_accepted_cb (TpCallChannel *channel,
    TpCallState state,
    TpCallFlags flags,
    TpCallStateReason *reason,
    GHashTable *details,
    Test *test)
{
  if (state == TP_CALL_STATE_ACCEPTED)
    g_main_loop_quit (test->mainloop);
}

static void
run_until_accepted (Test *test)
{
  guint id;

  tp_call_channel_accept_async (test->call_chan, NULL, NULL);

  id = g_signal_connect (test->call_chan, "state-changed",
      G_CALLBACK (run_until_accepted_cb), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->call_chan, id);
}

static void
hangup_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  g_clear_error (&test->error);

  tp_call_channel_hangup_finish (test->call_chan, result, &test->error);
  g_main_loop_quit (test->mainloop);
}

static void
add_content_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  g_clear_error (&test->error);
  tp_clear_object (&test->added_content);

  test->added_content = tp_call_channel_add_content_finish (test->call_chan,
      result, &test->error);
  g_main_loop_quit (test->mainloop);
}

/*
static void
content_remove_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  g_clear_error (&test->error);

  tp_call_content_remove_finish ((TpCallContent *) object,
      result, &test->error);
  g_main_loop_quit (test->mainloop);
}
*/

static void
test_basics (Test *test,
             gconstpointer data G_GNUC_UNUSED)
{
  GPtrArray *contents;
  GPtrArray *streams;
  TpCallContent *audio_content;
  TpCallContent *video_content;
  TpCallStream *audio_stream;
  TpCallStream *video_stream;
  GHashTable *remote_members;
  gpointer v;

  outgoing_call (test, "basic-test", TRUE, FALSE);
  assert_call_properties (test->call_chan,
      TP_CALL_STATE_PENDING_INITIATOR, 0,
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
      TRUE, 0,              /* call flags */
      TRUE, TRUE, FALSE);  /* initial audio/video must be what we said */

  /* We have one audio content but it's not active just yet */

  contents = tp_call_channel_get_contents (test->call_chan);
  g_assert_cmpuint (contents->len, ==, 1);

  audio_content = g_ptr_array_index (contents, 0);
  tp_tests_proxy_run_until_prepared (audio_content, NULL);
  assert_content_properties (audio_content,
      TP_MEDIA_STREAM_TYPE_AUDIO,
      TP_CALL_CONTENT_DISPOSITION_INITIAL);

  streams = tp_call_content_get_streams (audio_content);
  g_assert_cmpuint (streams->len, ==, 1);

  audio_stream = g_ptr_array_index (streams, 0);
  tp_tests_proxy_run_until_prepared (audio_stream, NULL);
  remote_members = tp_call_stream_get_remote_members (audio_stream);
  g_assert_cmpuint (g_hash_table_size (remote_members), ==, 1);
  v = g_hash_table_lookup (remote_members,
      tp_channel_get_target_contact (test->chan));
  g_assert_cmpuint (GPOINTER_TO_UINT (v), ==,
      TP_SENDING_STATE_PENDING_SEND);

  g_assert_cmpuint (tp_call_stream_get_local_sending_state (audio_stream),
      ==, TP_SENDING_STATE_SENDING);

  /* OK, that looks good. Actually make the call */
  tp_call_channel_accept_async (test->call_chan, accept_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Calling Accept again makes no sense, but mustn't crash */
  tp_call_channel_accept_async (test->call_chan, accept_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_NOT_AVAILABLE);
  g_clear_error (&test->error);

  /* Wait for the remote contact to answer, if they haven't already */
  run_until_answered (test);

  /* Calling Accept again makes no sense, but mustn't crash */
  tp_call_channel_accept_async (test->call_chan, accept_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_NOT_AVAILABLE);
  g_clear_error (&test->error);

  /* Check the call state. */
  assert_call_properties (test->call_chan,
      TP_CALL_STATE_ACCEPTED, tp_channel_get_handle (test->chan, NULL),
      TP_CALL_STATE_CHANGE_REASON_PROGRESS_MADE, "",
      TRUE, 0,              /* call flags */
      FALSE, FALSE, FALSE); /* don't care about initial audio/video */

  /* Connecting endpoints makes state become active */
  run_until_active (test);
  assert_call_properties (test->call_chan,
      TP_CALL_STATE_ACTIVE, test->self_handle,
      TP_CALL_STATE_CHANGE_REASON_PROGRESS_MADE, "",
      TRUE, 0,              /* call flags */
      FALSE, FALSE, FALSE); /* don't care about initial audio/video */

  /* There's still one content */
  contents = tp_call_channel_get_contents (test->call_chan);
  g_assert_cmpuint (contents->len, ==, 1);
  g_assert (g_ptr_array_index (contents, 0) == audio_content);

  /* Other contact is sending now */
  remote_members = tp_call_stream_get_remote_members (audio_stream);
  g_assert_cmpuint (g_hash_table_size (remote_members), == , 1);
  v = g_hash_table_lookup (remote_members,
      tp_channel_get_target_contact (test->chan));
  g_assert_cmpuint (GPOINTER_TO_UINT (v), ==, TP_SENDING_STATE_SENDING);
  g_assert_cmpuint (tp_call_stream_get_local_sending_state (audio_stream),
      ==, TP_SENDING_STATE_SENDING);

  /* AddContent with bad content-type must fail */

  tp_call_channel_add_content_async (test->call_chan,
      "", 31337, TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL,
      add_content_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert (test->error != NULL);
  g_assert (test->added_content == NULL);
  g_clear_error (&test->error);

  /* AddContent with bad initial-direction must fail */

  tp_call_channel_add_content_async (test->call_chan,
      "", TP_MEDIA_STREAM_TYPE_AUDIO, 31337,
      add_content_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert (test->error != NULL);
  g_assert (test->added_content == NULL);
  g_clear_error (&test->error);

   /* AddContent again, to add a video stream */

  tp_call_channel_add_content_async (test->call_chan,
      "", TP_MEDIA_STREAM_TYPE_VIDEO, TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL,
      add_content_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->added_content != NULL);
  video_content = test->added_content;
  tp_tests_proxy_run_until_prepared (video_content, NULL);

  /* There are two Contents, because now we have the video content too */
  contents = tp_call_channel_get_contents (test->call_chan);
  g_assert_cmpuint (contents->len, ==, 2);

  /* they could be either way round */
  if (g_ptr_array_index (contents, 0) == audio_content)
    {
      g_assert (g_ptr_array_index (contents, 1) == video_content);
    }
  else
    {
      g_assert (g_ptr_array_index (contents, 0) == video_content);
      g_assert (g_ptr_array_index (contents, 1) == audio_content);
    }

  assert_content_properties (video_content,
      TP_MEDIA_STREAM_TYPE_VIDEO,
      TP_CALL_CONTENT_DISPOSITION_NONE);

  streams = tp_call_content_get_streams (video_content);
  g_assert (streams != NULL);
  g_assert_cmpuint (streams->len, ==, 1);

  video_stream = g_ptr_array_index (streams, 0);
  tp_tests_proxy_run_until_prepared (video_stream, NULL);

  g_assert_cmpuint (tp_call_stream_get_local_sending_state (video_stream),
      ==, TP_SENDING_STATE_SENDING);

  remote_members = tp_call_stream_get_remote_members (video_stream);
  g_assert_cmpuint (g_hash_table_size (remote_members), ==, 1);
  v = g_hash_table_lookup (remote_members,
      tp_channel_get_target_contact (test->chan));

  /* After a moment, the video stream becomes connected, and the remote user
   * accepts our proposed direction change. These might happen in either
   * order, at least in this implementation. */

  if (GPOINTER_TO_UINT (v) != TP_SENDING_STATE_SENDING)
    g_assert_cmpuint (GPOINTER_TO_UINT (v), ==,
        TP_SENDING_STATE_PENDING_SEND);

#if 0
  /* FIXME: Content.Remove() is not implemented in example CM */

  /* Drop the video content */

  tp_call_content_remove_async (video_content,
      content_remove_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Get contents again: now there's only the audio */

  contents = tp_call_channel_get_contents (test->call_chan);
  g_assert_cmpuint (contents->len, ==, 1);
  g_assert (g_ptr_array_index (contents, 0) == audio_content);
#endif

  /* Hang up the call in the recommended way */

  tp_call_channel_hangup_async (test->call_chan,
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "", "",
      hangup_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  assert_ended_and_run_close (test, test->self_handle,
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
      "");
}

static void
test_no_answer (Test *test,
                gconstpointer data G_GNUC_UNUSED)
{
  /* This identifier contains the magic string (no answer), which means the
   * example will never answer. */
  outgoing_call (test, "smcv (no answer)", TRUE, FALSE);

  tp_call_channel_accept_async (test->call_chan, accept_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* After the initial flurry of D-Bus messages, smcv still hasn't answered */
  tp_tests_proxy_run_until_dbus_queue_processed (test->conn);

  assert_call_properties (test->call_chan,
      TP_CALL_STATE_INITIALISED, test->self_handle,
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
      TRUE, 0,              /* call flags */
      TRUE, TRUE, FALSE);  /* initial audio/video must be TRUE, FALSE */

  /* assume we're never going to get an answer, and hang up */
  tp_call_channel_hangup_async (test->call_chan,
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "", "",
      hangup_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  assert_ended_and_run_close (test, test->self_handle,
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
      "");
}

static void
test_busy (Test *test,
           gconstpointer data G_GNUC_UNUSED)
{
  /* This identifier contains the magic string (busy), which means the example
   * will simulate rejection of the call as busy rather than accepting it. */
  outgoing_call (test, "Robot101 (busy)", TRUE, FALSE);

  tp_call_channel_accept_async (test->call_chan, accept_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Wait for the remote contact to end the call as busy */
  run_until_ended (test);
  assert_ended_and_run_close (test, tp_channel_get_handle (test->chan, NULL),
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
      TP_ERROR_STR_BUSY);
}

static void
test_terminated_by_peer (Test *test,
                         gconstpointer data G_GNUC_UNUSED)
{
  /* This contact contains the magic string "(terminate)", meaning the example
   * simulates answering the call but then terminating it */
  outgoing_call (test, "The Governator (terminate)", TRUE, TRUE);

  tp_call_channel_accept_async (test->call_chan, accept_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Wait for the remote contact to answer, if they haven't already */

  run_until_answered (test);

  /* After that, the remote contact immediately ends the call */
  run_until_ended (test);
  assert_ended_and_run_close (test, tp_channel_get_handle (test->chan, NULL),
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
      "");
}

static void
test_terminate_via_close (Test *test,
                          gconstpointer data G_GNUC_UNUSED)
{
  outgoing_call (test, "basic-test", FALSE, TRUE);

  tp_call_channel_accept_async (test->call_chan, accept_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Wait for the remote contact to answer, if they haven't already */

  run_until_answered (test);

  assert_call_properties (test->call_chan,
      TP_CALL_STATE_ACCEPTED, test->peer_handle,
      TP_CALL_STATE_CHANGE_REASON_PROGRESS_MADE, "",
      TRUE, 0,              /* call flags */
      TRUE, FALSE, TRUE);  /* initial audio/video must be FALSE, TRUE */

  /* Terminate the call unceremoniously, by calling Close. This is not a
   * graceful hangup; rather, it's what the ChannelDispatcher would do to
   * signal a client crash, undispatchability, or whatever */

  tp_channel_close_async (test->chan, close_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* In response to termination, the channel does genuinely close */
  tp_tests_proxy_run_until_dbus_queue_processed (test->conn);
  g_assert (tp_proxy_get_invalidated (test->chan) != NULL);

  /* FIXME: when we hook up signals, check for expected call state
   * transition before invalidation */
}

/* FIXME: try removing the last stream, it should fail */

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
      GError *error = NULL;

      /* we only expect to receive one call */
      g_assert (test->chan == NULL);

      test->chan = tp_simple_client_factory_ensure_channel (test->factory,
          conn, object_path, properties, &error);
      g_assert_no_error (error);

      g_assert (TP_IS_CALL_CHANNEL (test->chan));
      test->call_chan = (TpCallChannel *) test->chan;

      g_assert_cmpint (tp_channel_get_requested (test->chan), ==, FALSE);
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

  tp_tests_proxy_run_until_prepared (test->chan, NULL);
}

static void
test_incoming (Test *test,
               gconstpointer data G_GNUC_UNUSED)
{
  GPtrArray *contents;
  TpCallContent *audio_content;

  trigger_incoming_call (test, "call me?", "caller");

  /* ring, ring! */
  assert_call_properties (test->call_chan,
      TP_CALL_STATE_INITIALISED, test->peer_handle,
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
      TRUE, 0,              /* call flags */
      TRUE, TRUE, FALSE);  /* initial audio/video must be TRUE, FALSE */

  /* Get Contents: we have an audio content */

  contents = tp_call_channel_get_contents (test->call_chan);
  g_assert_cmpuint (contents->len, ==, 1);
  audio_content = g_ptr_array_index (contents, 0);
  tp_tests_proxy_run_until_prepared (audio_content, NULL);
  g_assert_cmpuint (tp_call_content_get_media_type (audio_content), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);

  /* FIXME: assert about the properties of the content and the stream */

  /* Accept the call */
  tp_call_channel_accept_async (test->call_chan, accept_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  assert_call_properties (test->call_chan,
      TP_CALL_STATE_ACCEPTED, test->self_handle,
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
      TRUE, 0,              /* call flags */
      TRUE, TRUE, FALSE);  /* initial audio/video are still TRUE, FALSE */

  /* FIXME: check for stream directionality changes */

  /* Hang up the call */
  tp_call_channel_hangup_async (test->call_chan,
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "", "",
      hangup_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  assert_ended_and_run_close (test, test->self_handle,
      TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "");
}

static void
send_tones_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;
  GError *error = NULL;

  tp_call_channel_send_tones_finish (test->call_chan, result, &error);
  g_assert_no_error (error);

  test->wait_count--;
  if (test->wait_count <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
dtmf_change_requested_cb (TpCallContent *content,
    guchar event,
    TpSendingState state,
    gpointer user_data,
    GObject *weak_object)
{
  /* Only PENDING states can be requested */
  g_assert (state == TP_SENDING_STATE_PENDING_SEND ||
            state == TP_SENDING_STATE_PENDING_STOP_SENDING);

  if (state == TP_SENDING_STATE_PENDING_SEND)
    {
      tp_cli_call_content_interface_media_call_acknowledge_dtmf_change (content,
          -1, event, TP_SENDING_STATE_SENDING, NULL, NULL, NULL, NULL);
    }
  else if (state == TP_SENDING_STATE_PENDING_STOP_SENDING)
    {
      tp_cli_call_content_interface_media_call_acknowledge_dtmf_change (content,
          -1, event, TP_SENDING_STATE_NONE, NULL, NULL, NULL, NULL);
    }
}

static void
test_dtmf (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GPtrArray *contents;
  TpCallContent *content;

  outgoing_call (test, "dtmf-badger", TRUE, FALSE);
  run_until_accepted (test);
  run_until_active (test);

  contents = tp_call_channel_get_contents (test->call_chan);
  g_assert (contents->len == 1);
  content = g_ptr_array_index (contents, 0);

  tp_cli_call_content_interface_media_connect_to_dtmf_change_requested (content,
      dtmf_change_requested_cb, test, NULL, NULL, NULL);

  tp_call_channel_send_tones_async (test->call_chan, "123456789", NULL,
      send_tones_cb, test);
  tp_call_channel_send_tones_async (test->call_chan, "ABCD", NULL,
      send_tones_cb, test);

  test->wait_count = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
}

static void
teardown (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  tp_cli_connection_run_disconnect (test->conn, -1, &test->error, NULL);
  g_assert_no_error (test->error);

  g_array_unref (test->audio_request);
  g_array_unref (test->video_request);
  g_array_unref (test->invalid_request);
  g_array_unref (test->stream_ids);

  tp_clear_object (&test->added_content);
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
  g_set_prgname ("call-channel");

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
  g_test_add ("/call/dtmf", Test, NULL, setup, test_dtmf,
      teardown);

  return g_test_run ();
}
