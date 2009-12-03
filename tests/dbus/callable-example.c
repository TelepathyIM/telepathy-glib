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

static void
test_assert_uu_hash_contains (GHashTable *hash,
                              guint key,
                              guint expected)
{
  gpointer v;

  if (!g_hash_table_lookup_extended (hash, GUINT_TO_POINTER (key), NULL, &v))
    g_error ("Expected %u => %u in hash table, but key was absent", key,
        expected);

  g_assert_cmpuint (GPOINTER_TO_UINT (v), ==, expected);
}

typedef struct
{
  TpIntSet *added;
  TpIntSet *removed;
  TpIntSet *local_pending;
  TpIntSet *remote_pending;
  GHashTable *details;
} GroupEvent;

static GroupEvent *
group_event_new (void)
{
  return g_slice_new0 (GroupEvent);
}

static void
group_event_destroy (GroupEvent *ge)
{
  if (ge->added != NULL)
    tp_intset_destroy (ge->added);

  if (ge->removed != NULL)
    tp_intset_destroy (ge->removed);

  if (ge->local_pending != NULL)
    tp_intset_destroy (ge->local_pending);

  if (ge->remote_pending != NULL)
    tp_intset_destroy (ge->remote_pending);

  if (ge->details != NULL)
    g_hash_table_destroy (ge->details);

  g_slice_free (GroupEvent, ge);
}

typedef enum
{
  STREAM_EVENT_ADDED,
  STREAM_EVENT_DIRECTION_CHANGED,
  STREAM_EVENT_ERROR,
  STREAM_EVENT_REMOVED,
  STREAM_EVENT_STATE_CHANGED
} StreamEventType;

typedef struct
{
  StreamEventType type;
  guint id;
  TpHandle contact;
  TpMediaStreamType media_type;
  TpMediaStreamDirection direction;
  TpMediaStreamPendingSend pending_send;
  TpMediaStreamError error;
  TpMediaStreamState state;
} StreamEvent;

static StreamEvent *
stream_event_new (void)
{
  return g_slice_new0 (StreamEvent);
}

static void
stream_event_destroy (StreamEvent *se)
{
  g_slice_free (StreamEvent, se);
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

  GSList *group_events;
  gulong members_changed_detailed_id;

  GSList *stream_events;

  guint audio_stream_id;
  guint video_stream_id;
  GHashTable *stream_directions;
  GHashTable *stream_pending_sends;
  GHashTable *stream_states;
} Test;

/* For debugging, if this test fails */
static void test_dump_stream_events (Test *test) G_GNUC_UNUSED;

static void test_dump_stream_events (Test *test)
{
  GSList *link;

  g_message ("Stream events (most recent first):");

  for (link = test->stream_events; link != NULL; link = link->next)
    {
      StreamEvent *se = link->data;

      switch (se->type)
        {
        case STREAM_EVENT_ADDED:
          g_message ("Stream %u added, contact#%u, media type %u",
              se->id, se->contact, se->media_type);
          break;

        case STREAM_EVENT_DIRECTION_CHANGED:
          g_message ("Stream %u sending=%c, receiving=%c",
              se->id,
              (se->direction & TP_MEDIA_STREAM_DIRECTION_SEND ? 'y'
               : (se->pending_send & TP_MEDIA_STREAM_PENDING_LOCAL_SEND ?
                  'p' : 'n')),
              (se->direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE ? 'y'
               : (se->pending_send & TP_MEDIA_STREAM_PENDING_REMOTE_SEND ?
                  'p' : 'n'))
              );
          break;

        case STREAM_EVENT_ERROR:
          g_message ("Stream %u failed with error %u", se->id, se->error);
          break;

        case STREAM_EVENT_REMOVED:
          g_message ("Stream %u removed", se->id);
          break;

        case STREAM_EVENT_STATE_CHANGED:
          g_message ("Stream %u changed to state %u", se->id, se->state);
          break;
        }
    }
}

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

  test->audio_stream_id = G_MAXUINT;
  test->video_stream_id = G_MAXUINT;

  test->stream_directions = g_hash_table_new (NULL, NULL);
  test->stream_pending_sends = g_hash_table_new (NULL, NULL);
  test->stream_states = g_hash_table_new (NULL, NULL);

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
  g_hash_table_insert (parameters, "simulation-delay",
      tp_g_value_slice_new_uint (0));

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

  g_hash_table_destroy (parameters);
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
  GroupEvent *ge = group_event_new ();

  /* just log the event */
  ge->added = tp_intset_from_array (added);
  ge->removed = tp_intset_from_array (removed);
  ge->local_pending = tp_intset_from_array (local_pending);
  ge->remote_pending = tp_intset_from_array (remote_pending);
  ge->details = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) tp_g_value_slice_free);
  tp_g_hash_table_update (ge->details, details,
      (GBoxedCopyFunc) g_strdup, (GBoxedCopyFunc) tp_g_value_slice_dup);

  test->group_events = g_slist_prepend (test->group_events, ge);
}

static void
stream_added_cb (TpChannel *chan G_GNUC_UNUSED,
                 guint id,
                 guint contact,
                 guint media_type,
                 gpointer user_data,
                 GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  StreamEvent *se = stream_event_new ();

  se->type = STREAM_EVENT_ADDED;
  se->id = id;
  se->contact = contact;
  se->media_type = media_type;

  test->stream_events = g_slist_prepend (test->stream_events, se);

  /* this initial state is mandated by telepathy-spec 0.17.22 */
  g_hash_table_insert (test->stream_directions, GUINT_TO_POINTER (id),
      GUINT_TO_POINTER (TP_MEDIA_STREAM_DIRECTION_RECEIVE));
  g_hash_table_insert (test->stream_pending_sends, GUINT_TO_POINTER (id),
      GUINT_TO_POINTER (TP_MEDIA_STREAM_PENDING_LOCAL_SEND));
  g_hash_table_insert (test->stream_states, GUINT_TO_POINTER (id),
      GUINT_TO_POINTER (TP_MEDIA_STREAM_STATE_DISCONNECTED));
}

static void
stream_direction_changed_cb (TpChannel *chan G_GNUC_UNUSED,
                             guint id,
                             guint direction,
                             guint pending_flags,
                             gpointer user_data,
                             GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  StreamEvent *se = stream_event_new ();

  se->type = STREAM_EVENT_DIRECTION_CHANGED;
  se->id = id;
  se->direction = direction;
  se->pending_send = pending_flags;

  test->stream_events = g_slist_prepend (test->stream_events, se);

  g_hash_table_insert (test->stream_directions, GUINT_TO_POINTER (id),
      GUINT_TO_POINTER (direction));
  g_hash_table_insert (test->stream_pending_sends, GUINT_TO_POINTER (id),
      GUINT_TO_POINTER (pending_flags));
}

static void
stream_error_cb (TpChannel *chan G_GNUC_UNUSED,
                 guint id,
                 guint error,
                 const gchar *message,
                 gpointer user_data,
                 GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  StreamEvent *se = stream_event_new ();

  se->type = STREAM_EVENT_ERROR;
  se->id = id;
  se->error = error;

  test->stream_events = g_slist_prepend (test->stream_events, se);
}

static void
stream_removed_cb (TpChannel *chan G_GNUC_UNUSED,
                   guint id,
                   gpointer user_data,
                   GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  StreamEvent *se = stream_event_new ();

  se->type = STREAM_EVENT_REMOVED;
  se->id = id;

  test->stream_events = g_slist_prepend (test->stream_events, se);

  g_hash_table_remove (test->stream_directions, GUINT_TO_POINTER (id));
  g_hash_table_remove (test->stream_pending_sends, GUINT_TO_POINTER (id));
  g_hash_table_remove (test->stream_states, GUINT_TO_POINTER (id));
}

static void
stream_state_changed_cb (TpChannel *chan G_GNUC_UNUSED,
                         guint id,
                         guint state,
                         gpointer user_data,
                         GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  StreamEvent *se = stream_event_new ();

  se->type = STREAM_EVENT_STATE_CHANGED;
  se->id = id;
  se->state = state;

  test->stream_events = g_slist_prepend (test->stream_events, se);

  g_hash_table_insert (test->stream_states, GUINT_TO_POINTER (id),
      GUINT_TO_POINTER (state));
}

static void
test_connect_channel_signals (Test *test)
{
  test->members_changed_detailed_id = g_signal_connect (test->chan,
      "group-members-changed-detailed",
      G_CALLBACK (members_changed_detailed_cb), test);

  tp_cli_channel_type_streamed_media_connect_to_stream_added (test->chan,
      stream_added_cb, test, NULL, NULL, NULL);
  tp_cli_channel_type_streamed_media_connect_to_stream_removed (test->chan,
      stream_removed_cb, test, NULL, NULL, NULL);
  tp_cli_channel_type_streamed_media_connect_to_stream_error (test->chan,
      stream_error_cb, test, NULL, NULL, NULL);
  tp_cli_channel_type_streamed_media_connect_to_stream_direction_changed (
      test->chan, stream_direction_changed_cb, test, NULL, NULL, NULL);
  tp_cli_channel_type_streamed_media_connect_to_stream_state_changed (
      test->chan, stream_state_changed_cb, test, NULL, NULL, NULL);
}

static void
outgoing_call (Test *test,
               const gchar *id)
{
  GHashTable *request = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE,
          G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT, TP_HANDLE_TYPE_CONTACT,
      TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, id,
      NULL);

  tp_cli_connection_interface_requests_call_create_channel (test->conn, -1,
      request, channel_created_cb, test, NULL, NULL);
  g_hash_table_destroy (request);
  request = NULL;
  g_main_loop_run (test->mainloop);

  tp_channel_call_when_ready (test->chan, channel_ready_cb, test);
  g_main_loop_run (test->mainloop);

  test_connect_channel_signals (test);
}

static void
maybe_pop_stream_direction (Test *test)
{
  while (test->stream_events != NULL)
    {
      StreamEvent *se = test->stream_events->data;

      if (se->type == STREAM_EVENT_DIRECTION_CHANGED)
        {
          stream_event_destroy (se);
          test->stream_events = g_slist_delete_link (test->stream_events,
              test->stream_events);
        }
      else
        {
          break;
        }
    }
}

static void
test_basics (Test *test,
             gconstpointer data G_GNUC_UNUSED)
{
  GValueArray *audio_info, *video_info;
  guint not_a_stream_id = 31337;
  GroupEvent *ge;
  StreamEvent *se;

  outgoing_call (test, "basic-test");

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

  test->audio_stream_id = g_value_get_uint (audio_info->values + 0);

  g_assert_cmpuint (g_value_get_uint (audio_info->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);
  /* Initially, the stream is disconnected, we're willing to send to the peer,
   * and we've asked the peer whether they will send to us too */
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 3), ==,
      TP_MEDIA_STREAM_STATE_DISCONNECTED);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 4), ==,
      TP_MEDIA_STREAM_DIRECTION_SEND);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 5), ==,
      TP_MEDIA_STREAM_PENDING_REMOTE_SEND);

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
      test->audio_stream_id);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);
  /* Don't assert about the state or the direction here - it might already have
   * changed to connected or bidirectional, respectively. */

  /* The two oldest stream events should be the addition of the audio stream,
   * and the change to the appropriate direction (StreamAdded does not signal
   * stream directionality) */

  g_assert_cmpuint (g_slist_length (test->stream_events), >=, 2);

  se = g_slist_nth_data (test->stream_events,
      g_slist_length (test->stream_events) - 1);
  g_assert_cmpuint (se->type, ==, STREAM_EVENT_ADDED);
  g_assert_cmpuint (se->id, ==, test->audio_stream_id);
  g_assert_cmpuint (se->contact, ==, tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (se->media_type, ==, TP_MEDIA_STREAM_TYPE_AUDIO);

  se = g_slist_nth_data (test->stream_events,
      g_slist_length (test->stream_events) - 2);
  g_assert_cmpuint (se->type, ==, STREAM_EVENT_DIRECTION_CHANGED);
  g_assert_cmpuint (se->id, ==, test->audio_stream_id);
  g_assert_cmpuint (se->direction, ==, TP_MEDIA_STREAM_DIRECTION_SEND);
  g_assert_cmpuint (se->pending_send, ==, TP_MEDIA_STREAM_PENDING_REMOTE_SEND);

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

  ge = g_slist_nth_data (test->group_events, 1);

  g_assert_cmpuint (tp_intset_size (ge->added), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->removed), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->remote_pending), ==, 1);
  g_assert (tp_intset_is_member (ge->remote_pending,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "actor", NULL), ==,
      test->self_handle);
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "change-reason", NULL), ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

  ge = g_slist_nth_data (test->group_events, 0);

  g_assert_cmpuint (tp_intset_size (ge->added), ==, 1);
  g_assert (tp_intset_is_member (ge->added,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_intset_size (ge->removed), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->remote_pending), ==, 0);
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "actor", NULL), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "change-reason", NULL), ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

  /* Immediately the call is accepted, the remote peer accepts our proposed
   * stream direction */
  test_connection_run_until_dbus_queue_processed (test->conn);

  se = g_slist_nth_data (test->stream_events, 0);
  g_assert_cmpuint (se->type, ==, STREAM_EVENT_DIRECTION_CHANGED);
  g_assert_cmpuint (se->id, ==, test->audio_stream_id);
  g_assert_cmpuint (se->direction, ==,
      TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL);
  g_assert_cmpuint (se->pending_send, ==, 0);

  test_assert_uu_hash_contains (test->stream_states, test->audio_stream_id,
      TP_MEDIA_STREAM_STATE_DISCONNECTED);
  test_assert_uu_hash_contains (test->stream_directions, test->audio_stream_id,
      TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL);
  test_assert_uu_hash_contains (test->stream_pending_sends,
      test->audio_stream_id, 0);

  /* The stream should either already be connected, or become connected after
   * a while */

  while (GPOINTER_TO_UINT (g_hash_table_lookup (test->stream_states,
        GUINT_TO_POINTER (test->audio_stream_id))) !=
        TP_MEDIA_STREAM_STATE_CONNECTED)
    {
      g_main_context_iteration (NULL, TRUE);
    }

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

  test->video_stream_id = g_value_get_uint (video_info->values + 0);

  g_assert_cmpuint (g_value_get_uint (video_info->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (video_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_VIDEO);
  g_assert_cmpuint (g_value_get_uint (video_info->values + 3), ==,
      TP_MEDIA_STREAM_STATE_DISCONNECTED);
  g_assert_cmpuint (g_value_get_uint (video_info->values + 4), ==,
      TP_MEDIA_STREAM_DIRECTION_SEND);
  g_assert_cmpuint (g_value_get_uint (video_info->values + 5), ==,
      TP_MEDIA_STREAM_PENDING_REMOTE_SEND);

  /* ListStreams again: now we have the video stream too */

  tp_cli_channel_type_streamed_media_call_list_streams (test->chan, -1,
      listed_streams_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  g_assert_cmpuint (test->list_streams_return->len, ==, 2);

  /* this might be the video or the audio - we'll have to find out */
  audio_info = g_ptr_array_index (test->list_streams_return, 0);

  if (g_value_get_uint (audio_info->values + 0) == test->audio_stream_id)
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
      test->audio_stream_id);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);
  g_assert_cmpuint (g_value_get_uint (video_info->values + 0), ==,
      test->video_stream_id);
  g_assert_cmpuint (g_value_get_uint (video_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_VIDEO);

  /* After a moment, the video stream becomes connected, and the remote user
   * accepts our proposed direction change. These might happen in either
   * order, at least in this implementation. */

  while (GPOINTER_TO_UINT (g_hash_table_lookup (test->stream_directions,
            GUINT_TO_POINTER (test->video_stream_id))) !=
          TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL ||
        GPOINTER_TO_UINT (g_hash_table_lookup (test->stream_states,
            GUINT_TO_POINTER (test->video_stream_id))) !=
          TP_MEDIA_STREAM_STATE_CONNECTED)
    {
      g_main_context_iteration (NULL, TRUE);
    }

  se = g_slist_nth_data (test->stream_events, 3);
  g_assert_cmpuint (se->type, ==, STREAM_EVENT_ADDED);
  g_assert_cmpuint (se->id, ==, test->video_stream_id);
  g_assert_cmpuint (se->contact, ==, tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (se->media_type, ==, TP_MEDIA_STREAM_TYPE_VIDEO);

  se = g_slist_nth_data (test->stream_events, 2);
  g_assert_cmpuint (se->type, ==, STREAM_EVENT_DIRECTION_CHANGED);
  g_assert_cmpuint (se->id, ==, test->video_stream_id);
  g_assert_cmpuint (se->direction, ==, TP_MEDIA_STREAM_DIRECTION_SEND);
  g_assert_cmpuint (se->pending_send, ==, TP_MEDIA_STREAM_PENDING_REMOTE_SEND);

  /* the most recent events, 0 and 1, are the direction change to bidirectional
   * and the state change to connected, in arbitrary order - we already checked
   * that they happened */

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
  g_array_append_val (test->stream_ids, test->video_stream_id);
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
      test->audio_stream_id);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);

  /* The last event should be the removal of the video stream */

  se = g_slist_nth_data (test->stream_events, 0);

  g_assert_cmpuint (se->type, ==, STREAM_EVENT_REMOVED);
  g_assert_cmpuint (se->id, ==, test->video_stream_id);

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
  ge = g_slist_nth_data (test->group_events, 0);

  g_assert_cmpuint (tp_intset_size (ge->added), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->removed), ==, 2);
  g_assert (tp_intset_is_member (ge->removed,
        test->self_handle));
  g_assert (tp_intset_is_member (ge->removed,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_intset_size (ge->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->remote_pending), ==, 0);
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "actor", NULL), ==,
      test->self_handle);
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "change-reason", NULL), ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

  /* The last stream event should be the removal of the audio stream */

  se = g_slist_nth_data (test->stream_events, 0);

  g_assert_cmpuint (se->type, ==, STREAM_EVENT_REMOVED);
  g_assert_cmpuint (se->id, ==, test->audio_stream_id);

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
  GroupEvent *ge;
  StreamEvent *se;

  /* This identifier contains the magic string (no answer), which means the
   * example will never answer. */
  outgoing_call (test, "smcv (no answer)");

  /* request an audio stream */
  tp_cli_channel_type_streamed_media_call_request_streams (test->chan, -1,
      tp_channel_get_handle (test->chan, NULL),
      test->audio_request, requested_streams_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  test_connection_run_until_dbus_queue_processed (test->conn);

  maybe_pop_stream_direction (test);
  g_assert_cmpuint (g_slist_length (test->stream_events), ==, 1);
  se = g_slist_nth_data (test->stream_events, 0);
  g_assert_cmpuint (se->type, ==, STREAM_EVENT_ADDED);
  test->audio_stream_id = se->id;

  /* After the initial flurry of D-Bus messages, smcv still hasn't answered */
  g_assert_cmpuint (tp_channel_group_get_self_handle (test->chan), ==,
      test->self_handle);
  g_assert_cmpuint (tp_channel_group_get_handle_owner (test->chan,
        test->self_handle), ==, test->self_handle);
  g_assert_cmpuint (tp_intset_size (tp_channel_group_get_members (test->chan)),
      ==, 1);
  g_assert_cmpuint (tp_intset_size (
        tp_channel_group_get_local_pending (test->chan)), ==, 0);
  g_assert_cmpuint (tp_intset_size (
        tp_channel_group_get_remote_pending (test->chan)), ==, 1);
  g_assert (tp_intset_is_member (tp_channel_group_get_members (test->chan),
        test->self_handle));
  g_assert (tp_intset_is_member (tp_channel_group_get_remote_pending (
          test->chan),
        tp_channel_get_handle (test->chan, NULL)));

  /* assume we're never going to get an answer, and hang up */
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
  ge = g_slist_nth_data (test->group_events, 0);

  g_assert_cmpuint (tp_intset_size (ge->added), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->removed), ==, 2);
  g_assert (tp_intset_is_member (ge->removed,
        test->self_handle));
  g_assert (tp_intset_is_member (ge->removed,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_intset_size (ge->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->remote_pending), ==, 0);
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "actor", NULL), ==,
      test->self_handle);
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "change-reason", NULL), ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);
}

static void
test_busy (Test *test,
           gconstpointer data G_GNUC_UNUSED)
{
  GroupEvent *ge;
  StreamEvent *se;

  /* This identifier contains the magic string (busy), which means the example
   * will simulate rejection of the call as busy rather than accepting it. */
  outgoing_call (test, "Robot101 (busy)");

  /* request an audio stream */
  tp_cli_channel_type_streamed_media_call_request_streams (test->chan, -1,
      tp_channel_get_handle (test->chan, NULL),
      test->audio_request, requested_streams_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  /* Wait for the remote contact to reject the call */
  while (tp_proxy_get_invalidated (test->chan) != NULL)
    {
      g_main_context_iteration (NULL, TRUE);
    }

  /* The last stream event should be the removal of the stream */

  test_connection_run_until_dbus_queue_processed (test->conn);

  se = g_slist_nth_data (test->stream_events, 0);
  g_assert_cmpuint (se->type, ==, STREAM_EVENT_REMOVED);

  /* The last event should be that the peer and the self-handle were both
   * removed by the peer, for reason BUSY */
  ge = g_slist_nth_data (test->group_events, 0);

  g_assert_cmpuint (tp_intset_size (ge->added), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->removed), ==, 2);
  g_assert (tp_intset_is_member (ge->removed,
        test->self_handle));
  g_assert (tp_intset_is_member (ge->removed,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_intset_size (ge->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->remote_pending), ==, 0);
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "actor", NULL), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "change-reason", NULL), ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_BUSY);
}

static void
test_terminated_by_peer (Test *test,
                         gconstpointer data G_GNUC_UNUSED)
{
  GroupEvent *ge;
  StreamEvent *se;

  /* This contact contains the magic string "(terminate)", meaning the example
   * simulates answering the call but then terminating it */
  outgoing_call (test, "The Governator (terminate)");

  /* request an audio stream */
  tp_cli_channel_type_streamed_media_call_request_streams (test->chan, -1,
      tp_channel_get_handle (test->chan, NULL),
      test->audio_request, requested_streams_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  /* Wait for the remote contact to answer, if they haven't already */

  while (!tp_intset_is_member (tp_channel_group_get_members (test->chan),
        tp_channel_get_handle (test->chan, NULL)))
    g_main_context_iteration (NULL, TRUE);

  /* After that, wait for the remote contact to end the call */
  while (tp_proxy_get_invalidated (test->chan) != NULL)
    {
      g_main_context_iteration (NULL, TRUE);
    }

  /* The last stream event should be the removal of the stream */

  test_connection_run_until_dbus_queue_processed (test->conn);

  se = g_slist_nth_data (test->stream_events, 0);
  g_assert_cmpuint (se->type, ==, STREAM_EVENT_REMOVED);

  /* The last event should be that the peer and the self-handle were both
   * removed by the peer, for no particular reason */
  ge = g_slist_nth_data (test->group_events, 0);

  g_assert_cmpuint (tp_intset_size (ge->added), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->removed), ==, 2);
  g_assert (tp_intset_is_member (ge->removed,
        test->self_handle));
  g_assert (tp_intset_is_member (ge->removed,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_intset_size (ge->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->remote_pending), ==, 0);
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "actor", NULL), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "change-reason", NULL), ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);
}

static void
test_terminate_via_close (Test *test,
                          gconstpointer data G_GNUC_UNUSED)
{
  GroupEvent *ge;
  StreamEvent *se;

  outgoing_call (test, "basic-test");

  /* request an audio stream */
  tp_cli_channel_type_streamed_media_call_request_streams (test->chan, -1,
      tp_channel_get_handle (test->chan, NULL),
      test->audio_request, requested_streams_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  test_connection_run_until_dbus_queue_processed (test->conn);

  maybe_pop_stream_direction (test);
  g_assert_cmpuint (g_slist_length (test->stream_events), ==, 1);
  se = g_slist_nth_data (test->stream_events, 0);
  g_assert_cmpuint (se->type, ==, STREAM_EVENT_ADDED);
  test->audio_stream_id = se->id;

  /* Wait for the remote contact to answer, if they haven't already */

  while (!tp_intset_is_member (tp_channel_group_get_members (test->chan),
        tp_channel_get_handle (test->chan, NULL)))
    g_main_context_iteration (NULL, TRUE);

  /* Hang up the call unceremoniously, by calling Close */

  tp_cli_channel_call_close (test->chan, -1, void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  /* In response to hanging up, the channel closes */
  test_connection_run_until_dbus_queue_processed (test->conn);
  g_assert (tp_proxy_get_invalidated (test->chan) != NULL);

  /* The last event should be that the peer and the self-handle were both
   * removed */
  ge = g_slist_nth_data (test->group_events, 0);

  g_assert_cmpuint (tp_intset_size (ge->added), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->removed), ==, 2);
  g_assert (tp_intset_is_member (ge->removed,
        test->self_handle));
  g_assert (tp_intset_is_member (ge->removed,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_intset_size (ge->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->remote_pending), ==, 0);
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "actor", NULL), ==,
      test->self_handle);
  g_assert_cmpuint (tp_asv_get_uint32 (ge->details, "change-reason", NULL), ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

  /* The last stream event should be the removal of the audio stream */

  se = g_slist_nth_data (test->stream_events, 0);

  g_assert_cmpuint (se->type, ==, STREAM_EVENT_REMOVED);
  g_assert_cmpuint (se->id, ==, test->audio_stream_id);
}

static void
test_terminate_via_no_streams (Test *test,
                               gconstpointer data G_GNUC_UNUSED)
{
  GroupEvent *ge;
  StreamEvent *se;

  outgoing_call (test, "basic-test");

  /* request an audio stream */
  tp_cli_channel_type_streamed_media_call_request_streams (test->chan, -1,
      tp_channel_get_handle (test->chan, NULL),
      test->audio_request, requested_streams_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  test_connection_run_until_dbus_queue_processed (test->conn);

  maybe_pop_stream_direction (test);
  g_assert_cmpuint (g_slist_length (test->stream_events), ==, 1);
  se = g_slist_nth_data (test->stream_events, 0);
  g_assert_cmpuint (se->type, ==, STREAM_EVENT_ADDED);
  test->audio_stream_id = se->id;

  /* Wait for the remote contact to answer, if they haven't already */

  while (!tp_intset_is_member (tp_channel_group_get_members (test->chan),
        tp_channel_get_handle (test->chan, NULL)))
    g_main_context_iteration (NULL, TRUE);

  /* Close the audio stream */

  g_array_set_size (test->stream_ids, 0);
  g_array_append_val (test->stream_ids, test->audio_stream_id);
  tp_cli_channel_type_streamed_media_call_remove_streams (test->chan, -1,
      test->stream_ids,
      void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

  /* In response to hanging up, the channel closes */
  test_connection_run_until_dbus_queue_processed (test->conn);
  g_assert (tp_proxy_get_invalidated (test->chan) != NULL);

  /* The last event should be that the peer and the self-handle were both
   * removed */
  ge = g_slist_nth_data (test->group_events, 0);

  g_assert_cmpuint (tp_intset_size (ge->added), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->removed), ==, 2);
  g_assert (tp_intset_is_member (ge->removed,
        test->self_handle));
  g_assert (tp_intset_is_member (ge->removed,
        tp_channel_get_handle (test->chan, NULL)));
  g_assert_cmpuint (tp_intset_size (ge->local_pending), ==, 0);
  g_assert_cmpuint (tp_intset_size (ge->remote_pending), ==, 0);

  /* The last stream event should be the removal of the audio stream */

  se = g_slist_nth_data (test->stream_events, 0);

  g_assert_cmpuint (se->type, ==, STREAM_EVENT_REMOVED);
  g_assert_cmpuint (se->id, ==, test->audio_stream_id);
}

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
      if (tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA))
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
      test_assert_no_error (test->error);
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
  test_assert_no_error (test->error);

  new_channels_sig =
    tp_cli_connection_interface_requests_connect_to_new_channels (test->conn,
        expect_incoming_call_cb, test, NULL, NULL, &test->error);
  test_assert_no_error (test->error);

  tp_cli_connection_interface_simple_presence_run_set_presence (test->conn, -1,
      "available", message, &test->error, NULL);
  test_assert_no_error (test->error);

  /* wait for the call to happen if it hasn't already */
  while (test->chan == NULL)
    {
      g_main_context_iteration (NULL, TRUE);
    }

  tp_proxy_signal_connection_disconnect (new_channels_sig);

  tp_channel_call_when_ready (test->chan, channel_ready_cb, test);
  g_main_loop_run (test->mainloop);
  test_connect_channel_signals (test);
}

static void
test_incoming (Test *test,
               gconstpointer data G_GNUC_UNUSED)
{
  GValueArray *audio_info;

  trigger_incoming_call (test, "call me?", "caller");

  /* At this point in the channel's lifetime, we should be in local-pending,
   * with the caller in members */
  g_assert_cmpuint (tp_channel_group_get_self_handle (test->chan), ==,
      test->self_handle);
  g_assert_cmpuint (tp_channel_group_get_handle_owner (test->chan,
        test->self_handle), ==, test->self_handle);
  g_assert_cmpuint (tp_intset_size (tp_channel_group_get_members (test->chan)),
      ==, 1);
  g_assert_cmpuint (tp_intset_size (
        tp_channel_group_get_local_pending (test->chan)), ==, 1);
  g_assert_cmpuint (tp_intset_size (
        tp_channel_group_get_remote_pending (test->chan)), ==, 0);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_local_pending (test->chan), test->self_handle));
  g_assert (tp_intset_is_member (tp_channel_group_get_members (test->chan),
        tp_channel_get_handle (test->chan, NULL)));

  /* ListStreams: we have an audio stream */

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

  test->audio_stream_id = g_value_get_uint (audio_info->values + 0);

  g_assert_cmpuint (g_value_get_uint (audio_info->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 1), ==,
      tp_channel_get_handle (test->chan, NULL));
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 2), ==,
      TP_MEDIA_STREAM_TYPE_AUDIO);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 3), ==,
      TP_MEDIA_STREAM_STATE_DISCONNECTED);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 4), ==,
      TP_MEDIA_STREAM_DIRECTION_RECEIVE);
  g_assert_cmpuint (g_value_get_uint (audio_info->values + 5), ==,
      TP_MEDIA_STREAM_PENDING_LOCAL_SEND);

  /* We already had the stream when the channel was created, so we'll have
   * missed the StreamAdded signal */
  g_hash_table_insert (test->stream_directions,
      GUINT_TO_POINTER (test->audio_stream_id),
      GUINT_TO_POINTER (TP_MEDIA_STREAM_DIRECTION_RECEIVE));
  g_hash_table_insert (test->stream_pending_sends,
      GUINT_TO_POINTER (test->audio_stream_id),
      GUINT_TO_POINTER (TP_MEDIA_STREAM_PENDING_LOCAL_SEND));
  g_hash_table_insert (test->stream_states,
      GUINT_TO_POINTER (test->audio_stream_id),
      GUINT_TO_POINTER (TP_MEDIA_STREAM_STATE_DISCONNECTED));

  /* Accept the call */
  g_array_set_size (test->contacts, 0);
  g_array_append_val (test->contacts, test->self_handle);
  tp_cli_channel_interface_group_call_add_members (test->chan,
      -1, test->contacts, "", void_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  test_assert_no_error (test->error);

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

  /* Immediately the call is accepted, we accept the remote peer's proposed
   * stream direction */
  test_connection_run_until_dbus_queue_processed (test->conn);

  test_assert_uu_hash_contains (test->stream_directions, test->audio_stream_id,
      TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL);
  test_assert_uu_hash_contains (test->stream_pending_sends,
      test->audio_stream_id, 0);

  /* The stream should either already be connected, or become connected after
   * a while */
  while (GPOINTER_TO_UINT (g_hash_table_lookup (test->stream_states,
        GUINT_TO_POINTER (test->audio_stream_id))) ==
        TP_MEDIA_STREAM_STATE_DISCONNECTED)
    {
      g_main_context_iteration (NULL, TRUE);
    }

  test_assert_uu_hash_contains (test->stream_states, test->audio_stream_id,
      TP_MEDIA_STREAM_STATE_CONNECTED);

  /* Hang up the call */
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
}

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
  g_array_free (test->invalid_request, TRUE);
  g_array_free (test->stream_ids, TRUE);
  g_array_free (test->contacts, TRUE);

  g_slist_foreach (test->group_events, (GFunc) group_event_destroy, NULL);
  g_slist_free (test->group_events);

  g_slist_foreach (test->stream_events, (GFunc) stream_event_destroy, NULL);
  g_slist_free (test->stream_events);

  CLEAR_BOXED (TP_ARRAY_TYPE_MEDIA_STREAM_INFO_LIST,
      &test->list_streams_return);
  CLEAR_BOXED (TP_ARRAY_TYPE_MEDIA_STREAM_INFO_LIST,
      &test->request_streams_return);

  g_hash_table_destroy (test->stream_directions);
  g_hash_table_destroy (test->stream_pending_sends);
  g_hash_table_destroy (test->stream_states);

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
  g_test_add ("/callable/busy", Test, NULL, setup, test_busy, teardown);
  g_test_add ("/callable/no-answer", Test, NULL, setup, test_no_answer,
      teardown);
  g_test_add ("/callable/terminated-by-peer", Test, NULL, setup,
      test_terminated_by_peer, teardown);
  g_test_add ("/callable/terminate-via-close", Test, NULL, setup,
      test_terminate_via_close, teardown);
  g_test_add ("/callable/terminate-via-no-streams", Test, NULL, setup,
      test_terminate_via_no_streams, teardown);
  g_test_add ("/callable/incoming", Test, NULL, setup, test_incoming,
      teardown);

  return g_test_run ();
}
