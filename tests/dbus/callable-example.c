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

#define CLEAR_BOXED(g, o) \
  G_STMT_START { \
      if (*(o) != NULL) \
        { \
          g_boxed_free ((g), *(o)); \
          *(o) = NULL; \
        } \
  } G_STMT_END

typedef struct
{
  TpIntSet *added;
  TpIntSet *removed;
  TpIntSet *local_pending;
  TpIntSet *remote_pending;
  GHashTable *details;
} Event;

static Event *
event_new (void)
{
  return g_slice_new0 (Event);
}

static void
event_destroy (Event *e)
{
  if (e->added != NULL)
    tp_intset_destroy (e->added);

  if (e->removed != NULL)
    tp_intset_destroy (e->removed);

  if (e->local_pending != NULL)
    tp_intset_destroy (e->local_pending);

  if (e->remote_pending != NULL)
    tp_intset_destroy (e->remote_pending);

  if (e->details != NULL)
    g_hash_table_destroy (e->details);

  g_slice_free (Event, e);
}

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

  GArray *audio_request;
  GArray *video_request;
  GArray *invalid_request;

  GArray *stream_ids;
  GArray *contacts;
  GPtrArray *request_streams_return;
  GPtrArray *list_streams_return;

  GSList *events;
  gulong members_changed_detailed_id;
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
  guint audio = TP_MEDIA_STREAM_TYPE_AUDIO;
  guint video = TP_MEDIA_STREAM_TYPE_VIDEO;
  guint not_a_media_type = 31337;

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

  test->audio_request = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
  g_array_append_val (test->audio_request, audio);

  test->video_request = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
  g_array_append_val (test->video_request, video);

  test->invalid_request = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
  g_array_append_val (test->invalid_request, not_a_media_type);

  test->stream_ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), 2);
  test->contacts = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);

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
requested_streams_cb (TpChannel *chan G_GNUC_UNUSED,
                      const GPtrArray *stream_info,
                      const GError *error,
                      gpointer user_data,
                      GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  CLEAR_BOXED (TP_ARRAY_TYPE_MEDIA_STREAM_INFO_LIST,
      &test->request_streams_return);

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
listed_streams_cb (TpChannel *chan G_GNUC_UNUSED,
                   const GPtrArray *stream_info,
                   const GError *error,
                   gpointer user_data,
                   GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  /* ListStreams shouldn't fail in any of these tests */
  test_assert_no_error (error);

  CLEAR_BOXED (TP_ARRAY_TYPE_MEDIA_STREAM_INFO_LIST,
      &test->list_streams_return);

  test->list_streams_return = g_boxed_copy (
      TP_ARRAY_TYPE_MEDIA_STREAM_INFO_LIST, stream_info);

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
members_changed_detailed_cb (TpChannel *chan G_GNUC_UNUSED,
                             const GArray *added,
                             const GArray *removed,
                             const GArray *local_pending,
                             const GArray *remote_pending,
                             GHashTable *details,
                             gpointer user_data)
{
  Test *test = user_data;
  Event *e = event_new ();

  /* just log the event */
  e->added = tp_intset_from_array (added);
  e->removed = tp_intset_from_array (removed);
  e->local_pending = tp_intset_from_array (local_pending);
  e->remote_pending = tp_intset_from_array (remote_pending);
  e->details = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) tp_g_value_slice_free);
  tp_g_hash_table_update (e->details, details,
      (GBoxedCopyFunc) g_strdup, (GBoxedCopyFunc) tp_g_value_slice_dup);

  test->events = g_slist_prepend (test->events, e);
}

static void
test_basics (Test *test,
             gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) tp_g_value_slice_free);
  GValueArray *audio_info, *video_info;
  guint audio_stream_id;
  guint video_stream_id;
  guint not_a_stream_id = 31337;
  Event *e;

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

  test->members_changed_detailed_id = g_signal_connect (test->chan,
      "group-members-changed-detailed",
      G_CALLBACK (members_changed_detailed_cb), test);

  /* At this point in the channel's lifetime, we should be the channel's
   * only member */
  g_assert_cmpuint (tp_channel_group_get_self_handle (test->chan), ==,
      test->self_handle);
  g_assert_cmpuint (tp_channel_group_get_handle_owner (test->chan,
        test->self_handle), ==, test->self_handle);
  g_assert_cmpuint (tp_intset_size (tp_channel_group_get_members (test->chan)),
      ==, 1);
  g_assert_cmpuint (tp_intset_size (
        tp_channel_group_get_local_pending (test->chan)), ==, 0);
  g_assert_cmpuint (tp_intset_size (
        tp_channel_group_get_remote_pending (test->chan)), ==, 0);
  g_assert (tp_intset_is_member (tp_channel_group_get_members (test->chan),
        test->self_handle));

  /* ListStreams: we have no streams yet */

  tp_cli_channel_type_streamed_media_call_list_streams (test->chan, -1,
      listed_streams_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  g_assert_cmpuint (test->list_streams_return->len, ==, 0);

  /* RequestStreams with bad handle must fail */

  tp_cli_channel_type_streamed_media_call_request_streams (test->chan, -1,
      test->self_handle,
      test->audio_request, requested_streams_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert (test->error != NULL);
  g_clear_error (&test->error);

  /* RequestStreams with bad request must fail */

  tp_cli_channel_type_streamed_media_call_request_streams (test->chan, -1,
      tp_channel_get_handle (test->chan, NULL),
      test->invalid_request, requested_streams_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert (test->error != NULL);
  g_clear_error (&test->error);

  /* RequestStreams */

  tp_cli_channel_type_streamed_media_call_request_streams (test->chan, -1,
      tp_channel_get_handle (test->chan, NULL),
      test->audio_request, requested_streams_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  g_assert_cmpuint (test->request_streams_return->len, ==, 1);
  audio_info = g_ptr_array_index (test->request_streams_return, 0);

  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 0));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 1));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 2));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 3));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 4));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 5));

  audio_stream_id = g_value_get_uint (audio_info->values + 0);

  g_assert_cmpuint (g_value_get_uint (audio_info->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 3), ==,
      TP_MEDIA_STREAM_STATE_DISCONNECTED);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 4), ==,
      TP_MEDIA_STREAM_DIRECTION_NONE);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 5), ==, 0);

  /* ListStreams again: now we have the audio stream */

  tp_cli_channel_type_streamed_media_call_list_streams (test->chan, -1,
      listed_streams_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  g_assert_cmpuint (test->list_streams_return->len, ==, 1);
  audio_info = g_ptr_array_index (test->list_streams_return, 0);

  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 0));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 1));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 2));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 3));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 4));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 5));

  g_assert_cmpuint (g_value_get_uint (audio_info->values + 0), ==,
      audio_stream_id);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 3), ==,
      TP_MEDIA_STREAM_STATE_DISCONNECTED);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 4), ==,
      TP_MEDIA_STREAM_DIRECTION_NONE);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 5), ==, 0);

  /* Wait for the remote contact to answer, if they haven't already */

  while (!tp_intset_is_member (tp_channel_group_get_members (test->chan),
        tp_channel_get_handle (test->chan, NULL)))
    g_main_context_iteration (NULL, TRUE);

  /* The self-handle and the peer are now the channel's members */
  g_assert_cmpuint (tp_channel_group_get_handle_owner (test->chan,
        test->self_handle), ==, test->self_handle);
  g_assert_cmpuint (tp_channel_group_get_handle_owner (test->chan,
        tp_channel_get_handle (test->chan, NULL)),
      ==, tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (tp_intset_size (tp_channel_group_get_members (test->chan)),
      ==, 2);
  g_assert_cmpuint (tp_intset_size (
        tp_channel_group_get_local_pending (test->chan)), ==, 0);
  g_assert_cmpuint (tp_intset_size (
        tp_channel_group_get_remote_pending (test->chan)), ==, 0);
  g_assert (tp_intset_is_member (tp_channel_group_get_members (test->chan),
        test->self_handle));
  g_assert (tp_intset_is_member (tp_channel_group_get_members (test->chan),
        tp_channel_get_handle (test->chan, NULL)));

  /* Look at the event log: what should have happened is that the remote
   * peer was added first to remote-pending, then to members. (The event
   * log is in reverse chronological order.) */

  e = g_slist_nth_data (test->events, 1);

  g_assert_cmpuint (tp_intset_size (e->added), ==, 0);
  g_assert_cmpuint (tp_intset_size (e->removed), ==, 0);
  g_assert_cmpuint (tp_intset_size (e->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (e->remote_pending), ==, 1);
  g_assert (tp_intset_is_member (e->remote_pending,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_asv_get_uint32 (e->details, "actor", NULL), ==,
      test->self_handle);
  g_assert_cmpuint (tp_asv_get_uint32 (e->details, "change-reason", NULL), ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

  e = g_slist_nth_data (test->events, 0);

  g_assert_cmpuint (tp_intset_size (e->added), ==, 1);
  g_assert (tp_intset_is_member (e->added,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_intset_size (e->removed), ==, 0);
  g_assert_cmpuint (tp_intset_size (e->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (e->remote_pending), ==, 0);
  g_assert_cmpuint (tp_asv_get_uint32 (e->details, "actor", NULL), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (tp_asv_get_uint32 (e->details, "change-reason", NULL), ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

  /* RequestStreams again, to add a video stream */

  tp_cli_channel_type_streamed_media_call_request_streams (test->chan, -1,
      tp_channel_get_handle (test->chan, NULL),
      test->video_request, requested_streams_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  g_assert_cmpuint (test->request_streams_return->len, ==, 1);

  video_info = g_ptr_array_index (test->request_streams_return, 0);

  g_assert (G_VALUE_HOLDS_UINT (video_info->values + 0));
  g_assert (G_VALUE_HOLDS_UINT (video_info->values + 1));
  g_assert (G_VALUE_HOLDS_UINT (video_info->values + 2));
  g_assert (G_VALUE_HOLDS_UINT (video_info->values + 3));
  g_assert (G_VALUE_HOLDS_UINT (video_info->values + 4));
  g_assert (G_VALUE_HOLDS_UINT (video_info->values + 5));

  video_stream_id = g_value_get_uint (video_info->values + 0);

  g_assert_cmpuint (g_value_get_uint (video_info->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (video_info->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (video_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_VIDEO);
  g_assert_cmpuint (g_value_get_uint (video_info->values + 3), ==,
      TP_MEDIA_STREAM_STATE_DISCONNECTED);
  g_assert_cmpuint (g_value_get_uint (video_info->values + 4), ==,
      TP_MEDIA_STREAM_DIRECTION_NONE);
  g_assert_cmpuint (g_value_get_uint (video_info->values + 5), ==, 0);

  /* ListStreams again: now we have the video stream too */

  tp_cli_channel_type_streamed_media_call_list_streams (test->chan, -1,
      listed_streams_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  g_assert_cmpuint (test->list_streams_return->len, ==, 2);

  /* this might be the video or the audio - we'll have to find out */
  audio_info = g_ptr_array_index (test->list_streams_return, 0);

  if (g_value_get_uint (audio_info->values + 0) == audio_stream_id)
    {
      /* our guess was right, so the other one must be the video */
      video_info = g_ptr_array_index (test->list_streams_return, 1);
    }
  else
    {
      /* we guessed wrong, compensate for that */
      video_info = audio_info;
      audio_info = g_ptr_array_index (test->list_streams_return, 1);
    }

  g_assert_cmpuint (g_value_get_uint (audio_info->values + 0), ==,
      audio_stream_id);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);
  g_assert_cmpuint (g_value_get_uint (video_info->values + 0), ==,
      video_stream_id);
  g_assert_cmpuint (g_value_get_uint (video_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_VIDEO);

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
  test_assert_no_error (test->error);

  /* List streams again: now there's only the audio */

  tp_cli_channel_type_streamed_media_call_list_streams (test->chan, -1,
      listed_streams_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  g_assert_cmpuint (test->list_streams_return->len, ==, 1);
  audio_info = g_ptr_array_index (test->list_streams_return, 0);

  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 0));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 1));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 2));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 3));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 4));
  g_assert (G_VALUE_HOLDS_UINT (audio_info->values + 5));

  g_assert_cmpuint (g_value_get_uint (audio_info->values + 0), ==,
      audio_stream_id);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);

  /* Hang up the call in the recommended way */

  g_array_set_size (test->contacts, 0);
  g_array_append_val (test->contacts, test->self_handle);
  tp_cli_channel_interface_group_call_remove_members_with_reason (test->chan,
      -1, test->contacts, "", TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
      void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  /* In response to hanging up, the channel closes */
  test_connection_run_until_dbus_queue_processed (test->conn);
  g_assert (tp_proxy_get_invalidated (test->chan) != NULL);

  /* The last event should be that the peer and the self-handle were both
   * removed */
  e = g_slist_nth_data (test->events, 0);

  g_assert_cmpuint (tp_intset_size (e->added), ==, 0);
  g_assert_cmpuint (tp_intset_size (e->removed), ==, 2);
  g_assert (tp_intset_is_member (e->removed,
        test->self_handle));
  g_assert (tp_intset_is_member (e->removed,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_intset_size (e->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (e->remote_pending), ==, 0);
  g_assert_cmpuint (tp_asv_get_uint32 (e->details, "actor", NULL), ==,
      test->self_handle);
  g_assert_cmpuint (tp_asv_get_uint32 (e->details, "change-reason", NULL), ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

  /* FIXME: untested things include:
   *
   * RequestStreamDirection
   * StreamDirectionChanged being emitted correctly (part of RSD)
   * RequestStreamDirection failing (invalid direction, stream ID)
   *
   * StreamAdded being emitted correctly
   * StreamRemoved being emitted correctly
   *
   * StreamStateChanged being emitted (???)
   *
   * The contact accepting the call
   *
   * The Group interface
   */
}

/* FIXME: add a special contact who never accepts the call, so it rings
 * forever, and test that */

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

  if (test->members_changed_detailed_id != 0)
    {
      g_signal_handler_disconnect (test->chan,
          test->members_changed_detailed_id);
    }

  g_array_free (test->audio_request, TRUE);
  g_array_free (test->video_request, TRUE);
  g_array_free (test->stream_ids, TRUE);
  g_array_free (test->contacts, TRUE);

  g_slist_foreach (test->events, (GFunc) event_destroy, NULL);
  g_slist_free (test->events);

  CLEAR_BOXED (TP_ARRAY_TYPE_MEDIA_STREAM_INFO_LIST,
      &test->list_streams_return);
  CLEAR_BOXED (TP_ARRAY_TYPE_MEDIA_STREAM_INFO_LIST,
      &test->request_streams_return);

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
