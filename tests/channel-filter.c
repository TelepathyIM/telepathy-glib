#include "config.h"

#include "telepathy-glib/channel-filter-internal.h"

#include "tests/lib/util.h"

typedef struct {
    TpChannelFilter *filter;
} Fixture;

static void
setup (Fixture *f,
    gconstpointer data)
{
  tp_debug_set_flags ("all");
}

static void
test_basics (Fixture *f,
    gconstpointer data)
{
  GVariant *vardict;
  /*
  gboolean valid;
  guint i;
  TpEntityType call_entity_types[] = { TP_ENTITY_TYPE_CONTACT,
      TP_ENTITY_TYPE_ROOM,
      TP_ENTITY_TYPE_NONE };
      */

  f->filter = tp_channel_filter_new_for_all_types ();
  vardict = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (g_variant_n_children (vardict), ==, 0);
  g_variant_unref (vardict);
  g_clear_object (&f->filter);

#if 0
  f->filter = tp_channel_filter_new_for_text_chats ();
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 2);
  g_assert_cmpstr (tp_asv_get_string (asv,
        TP_PROP_CHANNEL_CHANNEL_TYPE), ==, TP_IFACE_CHANNEL_TYPE_TEXT);
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid),
      ==, TP_ENTITY_TYPE_CONTACT);
  g_assert (valid);
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_text_chatrooms ();
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 2);
  g_assert_cmpstr (tp_asv_get_string (asv,
        TP_PROP_CHANNEL_CHANNEL_TYPE), ==, TP_IFACE_CHANNEL_TYPE_TEXT);
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid),
      ==, TP_ENTITY_TYPE_ROOM);
  g_assert (valid);
  g_clear_object (&f->filter);

  for (i = 0; i < G_N_ELEMENTS (call_handle_types); i++)
    {
      f->filter = tp_channel_filter_new_for_calls (call_handle_types[i]);
      asv = _tp_channel_filter_use (f->filter);
      g_assert_cmpuint (tp_asv_size (asv), ==, 2);
      g_assert_cmpstr (tp_asv_get_string (asv,
            TP_PROP_CHANNEL_CHANNEL_TYPE), ==, TP_IFACE_CHANNEL_TYPE_CALL);
      g_assert_cmpuint (tp_asv_get_uint32 (asv,
            TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid),
          ==, call_handle_types[i]);
      g_assert (valid);
      g_clear_object (&f->filter);
    }

  f->filter = tp_channel_filter_new_for_stream_tubes ("rfb");
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 2);
  g_assert_cmpstr (tp_asv_get_string (asv,
        TP_PROP_CHANNEL_CHANNEL_TYPE), ==, TP_IFACE_CHANNEL_TYPE_STREAM_TUBE);
  g_assert_cmpstr (tp_asv_get_string (asv,
        TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE), ==, "rfb");
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_dbus_tubes ("com.example.Chess");
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 2);
  g_assert_cmpstr (tp_asv_get_string (asv,
        TP_PROP_CHANNEL_CHANNEL_TYPE), ==, TP_IFACE_CHANNEL_TYPE_DBUS_TUBE);
  g_assert_cmpstr (tp_asv_get_string (asv,
        TP_PROP_CHANNEL_TYPE_DBUS_TUBE_SERVICE_NAME), ==, "com.example.Chess");
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_file_transfers (NULL);
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 2);
  g_assert_cmpstr (tp_asv_get_string (asv,
        TP_PROP_CHANNEL_CHANNEL_TYPE), ==, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER);
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid),
      ==, TP_ENTITY_TYPE_CONTACT);
  g_assert (valid);
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_file_transfers ("com.example.AbiWord");
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 3);
  g_assert_cmpstr (tp_asv_get_string (asv,
        TP_PROP_CHANNEL_CHANNEL_TYPE), ==, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER);
  g_assert_cmpstr (tp_asv_get_string (asv,
        /* our constant naming strategy is unstoppable */
        TP_PROP_CHANNEL_INTERFACE_FILE_TRANSFER_METADATA_SERVICE_NAME), ==,
      "com.example.AbiWord");
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid),
      ==, TP_ENTITY_TYPE_CONTACT);
  g_assert (valid);
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_all_types ();
  tp_channel_filter_require_target_is_contact (f->filter);
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 1);
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid),
      ==, TP_ENTITY_TYPE_CONTACT);
  g_assert (valid);
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_all_types ();
  tp_channel_filter_require_target_is_room (f->filter);
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 1);
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid),
      ==, TP_ENTITY_TYPE_ROOM);
  g_assert (valid);
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_all_types ();
  tp_channel_filter_require_no_target (f->filter);
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 1);
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid),
      ==, TP_ENTITY_TYPE_NONE);
  g_assert (valid);
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_all_types ();
  tp_channel_filter_require_target_type (f->filter, TP_ENTITY_TYPE_ROOM);
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 1);
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid),
      ==, TP_ENTITY_TYPE_ROOM);
  g_assert (valid);
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_all_types ();
  tp_channel_filter_require_channel_type (f->filter, "com.example.Bees");
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 1);
  g_assert_cmpstr (tp_asv_get_string (asv,
        TP_PROP_CHANNEL_CHANNEL_TYPE), ==, "com.example.Bees");
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_all_types ();
  tp_channel_filter_require_locally_requested (f->filter, TRUE);
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 1);
  g_assert_cmpuint (tp_asv_get_boolean (asv,
        TP_PROP_CHANNEL_REQUESTED, &valid), ==, TRUE);
  g_assert (valid);
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_all_types ();
  tp_channel_filter_require_locally_requested (f->filter, FALSE);
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 1);
  g_assert_cmpuint (tp_asv_get_boolean (asv,
        TP_PROP_CHANNEL_REQUESTED, &valid), ==, FALSE);
  g_assert (valid);
  g_clear_object (&f->filter);

  f->filter = tp_channel_filter_new_for_all_types ();
  tp_channel_filter_require_property (f->filter,
      "com.example.Answer", g_variant_new_uint32 (42));
  asv = _tp_channel_filter_use (f->filter);
  g_assert_cmpuint (tp_asv_size (asv), ==, 1);
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        "com.example.Answer", &valid), ==, 42);
  g_assert (valid);
  g_clear_object (&f->filter);
#endif
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
  g_clear_object (&f->filter);
}

int
main (int argc,
    char **argv)
{
#define TEST_PREFIX "/channel-filter/"

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add (TEST_PREFIX "basics", Fixture, NULL, setup, test_basics,
      teardown);

  return g_test_run ();
}
