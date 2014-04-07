/* Feature test for TpContact creation.
 *
 * Code missing coverage in contact.c:
 * - connection becoming invalid
 * - fatal error on the connection
 * - inconsistent CM
 * - having to fall back to RequestAliases
 * - get_contacts_by_id with features (but it's trivial)
 *
 * Copyright © 2008–2011 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2008 Nokia Corporation
 * Copyright © 2007 Will Thompson
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/asv.h>
#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/value-array.h>

#include "telepathy-glib/reentrants.h"

#include "tests/lib/contacts-conn.h"
#include "tests/lib/broken-client-types-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

#define MEMBERS_CHANGED_MATCH_RULE \
  "type='signal'," \
  "interface='" TP_IFACE_CHANNEL_INTERFACE_GROUP "'," \
  "member='MembersChanged'"

typedef struct {
    GMainLoop *loop;
    GError *error /* initialized to 0 */;
    GPtrArray *contacts;
    gint waiting;
} Result;

typedef struct {
  Result result;
  TpBaseConnection *base_connection;
  TpTestsContactsConnection *service_conn;
  TpHandleRepoIface *service_repo;
  TpConnection *client_conn;
  GArray *all_contact_features;
} Fixture;

static void
finish (gpointer r)
{
  Result *result = r;

  result->waiting--;
  if (result->waiting <= 0)
    g_main_loop_quit (result->loop);
}

static void
reset_result (Result *result)
{
  tp_clear_pointer (&result->contacts, g_ptr_array_unref);
  g_clear_error (&result->error);
}

static void
upgrade_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpConnection *connection = (TpConnection *) source;
  Result *r = user_data;

  g_assert (r->contacts == NULL);
  g_assert (r->error == NULL);

  tp_connection_upgrade_contacts_finish (connection, result,
      &r->contacts, &r->error);
  g_main_loop_quit (r->loop);
}

static void
contact_info_verify (TpContact *contact)
{
  GList *info;
  TpContactInfoField *field;

  g_assert (tp_contact_has_feature (contact, TP_CONTACT_FEATURE_CONTACT_INFO));

  info = tp_contact_dup_contact_info (contact);
  g_assert (info != NULL);
  g_assert (info->data != NULL);
  g_assert (info->next == NULL);

  field = info->data;
  g_assert_cmpstr (field->field_name, ==, "n");
  g_assert (field->parameters != NULL);
  g_assert (field->parameters[0] == NULL);
  g_assert (field->field_value != NULL);
  g_assert_cmpstr (field->field_value[0], ==, "Foo");
  g_assert (field->field_value[1] == NULL);

  tp_contact_info_list_free (info);
}

static void
contact_info_notify_cb (TpContact *contact,
    GParamSpec *pspec,
    Result *result)
{
  contact_info_verify (contact);
  finish (result);
}

static void
contact_info_prepare_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpConnection *connection = TP_CONNECTION (object);
  Result *result = user_data;

  if (tp_proxy_prepare_finish (connection, res, &result->error))
    {
      TpContactInfoFlags flags;
      GList *specs, *l;

      flags = tp_connection_get_contact_info_flags (connection);
      g_assert_cmpint (flags, ==, TP_CONTACT_INFO_FLAG_PUSH |
          TP_CONTACT_INFO_FLAG_CAN_SET);

      specs = tp_connection_dup_contact_info_supported_fields (connection);
      g_assert_cmpuint (g_list_length (specs), ==, 5);

      for (l = specs; l != NULL; l = l->next)
        {
          TpContactInfoFieldSpec *spec = l->data;

          if (!tp_strdiff (spec->name, "bday") ||
              !tp_strdiff (spec->name, "fn"))
            {
              g_assert (spec->parameters != NULL);
              g_assert (spec->parameters[0] == NULL);
              g_assert_cmpint (spec->flags, ==, 0);
              g_assert_cmpint (spec->max, ==, 1);
            }
          else if (!tp_strdiff (spec->name, "email") ||
                   !tp_strdiff (spec->name, "tel") ||
                   !tp_strdiff (spec->name, "url"))
            {
              g_assert (spec->parameters != NULL);
              g_assert (spec->parameters[0] == NULL);
              g_assert_cmpint (spec->flags, ==, 0);
              g_assert_cmpint (spec->max, ==, G_MAXUINT32);
            }
          else
            {
              g_assert_not_reached ();
            }
        }

      tp_contact_info_spec_list_free (specs);
    }

  finish (result);
}

static void
contact_info_set_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpConnection *connection = TP_CONNECTION (object);
  Result *result = user_data;

  tp_connection_set_contact_info_finish (connection, res, &result->error);
  finish (result);
}

static void
contact_info_request_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpContact *contact = TP_CONTACT (object);
  Result *result = user_data;

  contact_info_verify (contact);

  tp_contact_request_contact_info_finish (contact, res, &result->error);
  finish (result);
}

static void
contact_info_request_cancelled_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpContact *contact = TP_CONTACT (object);
  Result *result = user_data;
  GError *error = NULL;

  tp_contact_request_contact_info_finish (contact, res, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);

  finish (result);
}

static gboolean
contact_info_request_cancel (gpointer cancellable)
{
  g_cancellable_cancel (cancellable);
  return FALSE;
}

static TpHandle
ensure_handle (Fixture *f,
    const gchar *id)
{
  TpBaseConnection *service_conn = (TpBaseConnection *) f->service_conn;
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (
      service_conn, TP_ENTITY_TYPE_CONTACT);

  return tp_handle_ensure (contact_repo, id, NULL, NULL);
}

static TpContact *
ensure_contact (Fixture *f,
    const gchar *id,
    TpHandle *handle)
{
  TpConnection *client_conn = f->client_conn;

  *handle = ensure_handle (f, id);
  return tp_connection_dup_contact_if_possible (client_conn, *handle, id);
}

static void
test_contact_info (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpTestsContactsConnection *service_conn = f->service_conn;
  TpConnection *client_conn = f->client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL };
  GQuark features[] = { TP_CONTACT_FEATURE_CONTACT_INFO, 0 };
  TpContact *contact;
  TpHandle handle;
  const gchar *field_value[] = { "Foo", NULL };
  GPtrArray *info;
  GList *info_list = NULL;
  GQuark conn_features[] = { TP_CONNECTION_FEATURE_CONTACT_INFO, 0 };
  GCancellable *cancellable;

  /* Create fake info fields */
  info = g_ptr_array_new_with_free_func ((GDestroyNotify) tp_value_array_free);
  g_ptr_array_add (info, tp_value_array_build (3,
      G_TYPE_STRING, "n",
      G_TYPE_STRV, NULL,
      G_TYPE_STRV, field_value,
      G_TYPE_INVALID));

  info_list = g_list_prepend (info_list,
      tp_contact_info_field_new ("n", NULL, (GStrv) field_value));

  tp_tests_contacts_connection_set_default_contact_info (service_conn, info);

  /* TEST1: Verify ContactInfo properties are correctly introspected on
   * TpConnection */
  tp_proxy_prepare_async (client_conn, conn_features, contact_info_prepare_cb,
      &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  /* TEST2: Set contact info on the connection, then get the self TpContact.
   * This tests the set operation works correctly and also test TpContact
   * correctly introspects the ContactInfo when the feature is requested. */
  contact = tp_connection_get_self_contact (client_conn);
  tp_connection_set_contact_info_async (client_conn, info_list,
    contact_info_set_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  tp_connection_upgrade_contacts_async (client_conn,
      1, &contact, features,
      upgrade_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  contact_info_verify (contact);

  reset_result (&result);

  /* TEST3: Create a TpContact with the INFO feature. Then change its info in
   * the CM. That should emit "notify::info" signal on the TpContact. */
  contact = ensure_contact (f, "info-test-3", &handle);
  tp_connection_upgrade_contacts_async (client_conn,
      1, &contact, features,
      upgrade_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  g_signal_connect (contact, "notify::contact-info",
      G_CALLBACK (contact_info_notify_cb), &result);

  tp_tests_contacts_connection_change_contact_info (service_conn, handle,
      info);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  reset_result (&result);
  g_object_unref (contact);

  /* TEST 4: First set the info in the CM for an handle, then create a TpContact
   * without INFO feature, and finally refresh the contact's info.
   *
   * We can't use ensure_contact() because that would create the contact
   * before we change its contact info, and changing its contact info
   * emits a signal. If we receive that signal while the contact exists,
   * we'll opportunistically fill in its contact info, and the assertion
   * that it has no contact info fails. */
  handle = ensure_handle (f, "info-test-4");
  tp_tests_contacts_connection_change_contact_info (service_conn, handle,
      info);
  tp_tests_proxy_run_until_dbus_queue_processed (client_conn);
  contact = tp_connection_dup_contact_if_possible (client_conn, handle,
      "info-test-4");

  tp_connection_upgrade_contacts_async (client_conn,
      1, &contact, NULL,
      upgrade_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  g_assert (tp_contact_dup_contact_info (contact) == NULL);

  g_signal_connect (contact, "notify::contact-info",
      G_CALLBACK (contact_info_notify_cb), &result);
  tp_connection_refresh_contact_info (client_conn, 1, &contact);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  reset_result (&result);
  g_object_unref (contact);

  /* TEST5: Create a TpContact without INFO feature, then request the contact's
   * info. */
  contact = ensure_contact (f, "info-test-5", &handle);
  tp_connection_upgrade_contacts_async (client_conn,
      1, &contact, NULL,
      upgrade_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  g_assert (tp_contact_dup_contact_info (contact) == NULL);

  tp_contact_request_contact_info_async (contact, NULL, contact_info_request_cb,
      &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  reset_result (&result);
  g_object_unref (contact);

  /* TEST6: Create a TpContact without INFO feature, then request the contact's
   * info, and cancel the request. */
  contact = ensure_contact (f, "info-test-6", &handle);
  tp_connection_upgrade_contacts_async (client_conn,
      1, &contact, NULL,
      upgrade_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  g_assert (tp_contact_dup_contact_info (contact) == NULL);

  cancellable = g_cancellable_new ();
  tp_contact_request_contact_info_async (contact, cancellable,
      contact_info_request_cancelled_cb, &result);

  g_idle_add_full (G_PRIORITY_HIGH, contact_info_request_cancel,
      cancellable, g_object_unref);

  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  reset_result (&result);
  g_object_unref (contact);

  /* Cleanup */
  g_main_loop_unref (result.loop);
  g_ptr_array_unref (info);
  tp_contact_info_list_free (info_list);
}

static void
prepare_avatar_requirements_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpConnection *connection = TP_CONNECTION (object);
  Result *result = user_data;

  if (tp_proxy_prepare_finish (connection, res, &result->error))
    {
      TpAvatarRequirements *req;

      req = tp_connection_get_avatar_requirements (connection);
      g_assert (req != NULL);
      g_assert (req->supported_mime_types != NULL);
      g_assert_cmpstr (req->supported_mime_types[0], ==, "image/png");
      g_assert (req->supported_mime_types[1] == NULL);
      g_assert_cmpuint (req->minimum_width, ==, 1);
      g_assert_cmpuint (req->minimum_height, ==, 2);
      g_assert_cmpuint (req->recommended_width, ==, 3);
      g_assert_cmpuint (req->recommended_height, ==, 4);
      g_assert_cmpuint (req->maximum_width, ==, 5);
      g_assert_cmpuint (req->maximum_height, ==, 6);
      g_assert_cmpuint (req->maximum_bytes, ==, 7);
    }

  finish (result);
}

static void
test_avatar_requirements (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpConnection *client_conn = f->client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL };
  GQuark features[] = { TP_CONNECTION_FEATURE_AVATAR_REQUIREMENTS, 0 };

  g_message (G_STRFUNC);

  tp_proxy_prepare_async (TP_PROXY (client_conn), features,
      prepare_avatar_requirements_cb, &result);
  g_main_loop_run (result.loop);

  g_assert_no_error (result.error);
  g_main_loop_unref (result.loop);
}

static TpContact *
create_contact_with_fake_avatar (Fixture *f,
    const gchar *id,
    gboolean request_avatar)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL };
  GQuark features[] = { TP_CONTACT_FEATURE_AVATAR_DATA, 0 };
  const gchar avatar_data[] = "fake-avatar-data";
  const gchar avatar_token[] = "fake-avatar-token";
  const gchar avatar_mime_type[] = "fake-avatar-mime-type";
  TpContact *contact;
  TpHandle handle;
  GArray *array;
  gchar *content = NULL;

  contact = ensure_contact (f, id, &handle);
  array = g_array_new (FALSE, FALSE, sizeof (gchar));
  g_array_append_vals (array, avatar_data, strlen (avatar_data) + 1);

  tp_tests_contacts_connection_change_avatar_data (f->service_conn, handle,
      array, avatar_mime_type, avatar_token);

  if (request_avatar)
    features[0] = TP_CONTACT_FEATURE_AVATAR_DATA;
  else
    features[0] = TP_CONTACT_FEATURE_AVATAR_TOKEN;

  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &contact, features,
      upgrade_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  g_assert_cmpstr (tp_contact_get_avatar_token (contact), ==, avatar_token);

  if (request_avatar)
    {
      GFile *avatar_file;

      /* If we requested avatar, it could come later */
      if (tp_contact_get_avatar_file (contact) == NULL)
        {
          g_signal_connect_swapped (contact, "notify::avatar-file",
              G_CALLBACK (finish), &result);
          g_main_loop_run (result.loop);
        }

      g_assert_cmpstr (tp_contact_get_avatar_mime_type (contact), ==,
          avatar_mime_type);

      avatar_file = tp_contact_get_avatar_file (contact);
      g_assert (avatar_file != NULL);
      g_file_load_contents (avatar_file, NULL, &content, NULL, NULL,
          &result.error);
      g_assert_no_error (result.error);
      g_assert_cmpstr (content, ==, avatar_data);
      g_free (content);
    }

  reset_result (&result);
  g_main_loop_unref (result.loop);

  g_array_unref (array);

  return contact;
}

static void
avatar_retrieved_cb (TpConnection *connection,
    guint handle,
    const gchar *token,
    const GArray *avatar,
    const gchar *mime_type,
    gpointer user_data,
    GObject *weak_object)
{
  gboolean *called = user_data;

  *called = TRUE;
}

/* From telepathy-haze, with permission */
static gboolean
haze_remove_directory (const gchar *path)
{
  const gchar *child_path;
  GDir *dir = g_dir_open (path, 0, NULL);
  gboolean ret = TRUE;

  if (!dir)
    return FALSE;

  while (ret && (child_path = g_dir_read_name (dir)))
    {
      gchar *child_full_path = g_build_filename (path, child_path, NULL);

      if (g_file_test (child_full_path, G_FILE_TEST_IS_DIR))
        {
          if (!haze_remove_directory (child_full_path))
            ret = FALSE;
        }
      else
        {
          DEBUG ("deleting %s", child_full_path);

          if (g_unlink (child_full_path))
            ret = FALSE;
        }

      g_free (child_full_path);
    }

  g_dir_close (dir);

  if (ret)
    {
      DEBUG ("deleting %s", path);
      ret = !g_rmdir (path);
    }

  return ret;
}

static void
test_avatar_data (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpConnection *client_conn = f->client_conn;
  gboolean avatar_retrieved_called;
  GError *error = NULL;
  TpContact *contact1, *contact2;
  TpProxySignalConnection *signal_id;

  g_message (G_STRFUNC);

  /* Check if AvatarRetrieved gets called */
  signal_id = tp_cli_connection_interface_avatars1_connect_to_avatar_retrieved (
      client_conn, avatar_retrieved_cb, &avatar_retrieved_called, NULL, NULL,
      &error);
  g_assert_no_error (error);

  /* First time we create a contact, avatar should not be in cache, so
   * AvatarRetrived should be called */
  avatar_retrieved_called = FALSE;
  contact1 = create_contact_with_fake_avatar (f, "fake-id1", TRUE);
  g_assert (avatar_retrieved_called);
  g_assert (contact1 != NULL);
  g_assert (tp_contact_get_avatar_file (contact1) != NULL);

  /* Second time we create a contact, avatar should be in cache now, so
   * AvatarRetrived should NOT be called */
  avatar_retrieved_called = FALSE;
  contact2 = create_contact_with_fake_avatar (f, "fake-id2", TRUE);
  g_assert (!avatar_retrieved_called);
  g_assert (contact2 != NULL);
  g_assert (tp_contact_get_avatar_file (contact2) != NULL);

  g_assert (g_file_equal (
      tp_contact_get_avatar_file (contact1),
      tp_contact_get_avatar_file (contact2)));

  tp_proxy_signal_connection_disconnect (signal_id);
  g_object_unref (contact1);
  g_object_unref (contact2);
}

static void
test_avatar_data_after_token (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  const gchar *id = "avatar-data-after-token";
  TpContact *contact1, *contact2;

  g_message (G_STRFUNC);

  /* Create a contact with AVATAR_TOKEN feature */
  contact1 = create_contact_with_fake_avatar (f, id, FALSE);
  g_assert (contact1 != NULL);
  g_assert (tp_contact_get_avatar_file (contact1) == NULL);

  /* Now create the same contact with AVATAR_DATA feature */
  contact2 = create_contact_with_fake_avatar (f, id, TRUE);
  g_assert (contact2 != NULL);
  g_assert (tp_contact_get_avatar_file (contact2) != NULL);

  g_assert (contact1 == contact2);

  /* Cleanup */
  g_object_unref (contact1);
  g_object_unref (contact2);
}

/* Silently removes the TpBaseConnection object from D-Bus, so that if the test
 * makes any D-Bus calls on it, it will fail (but the TpConnection proxy isn't
 * invalidated otherwise)
 */
static void
make_the_connection_disappear (Fixture *f)
{
  GError *error = NULL;
  gboolean ok;

  tp_dbus_connection_unregister_object (
      tp_base_connection_get_dbus_connection (f->base_connection),
      f->base_connection);
  /* check that that worked */
  ok = tp_cli_connection_run_connect (f->client_conn, -1,
      &error, NULL);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
  g_assert (!ok);
  g_clear_error (&error);
}

/* Returns the TpBaseConnection to D-Bus (after a previous call to
 * make_the_connection_disappear())
 */
static void
put_the_connection_back (Fixture *f)
{
  GError *error = NULL;
  gboolean ok;

  tp_dbus_connection_register_object (
      tp_base_connection_get_dbus_connection (f->base_connection),
      tp_base_connection_get_object_path (f->base_connection),
      f->base_connection);
  /* check that *that* worked */
  ok = tp_cli_connection_run_connect (f->client_conn, -1,
      &error, NULL);
  g_assert_no_error (error);
  g_assert (ok);
}

static TpContact *
ensure_contact_no_caps (Fixture *f,
    const gchar *id,
    TpHandle *handle)
{
  TpContact *contact;
  GHashTable *capabilities;

  contact = ensure_contact (f, id, handle);

  /* Unlike almost every other feature, with capabilities “not sure” and “none”
   * are different: you really might care about the difference between “I don't
   * know if blah can do video” versus “I know blah cannot do video”.
   *
   * It happens that we get the repeated-reintrospection behaviour for the
   * former case of contact caps. I can't really be bothered to fix this.
   */
  capabilities = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_ptr_array_unref);
  g_hash_table_insert (capabilities, GUINT_TO_POINTER (*handle),
      g_ptr_array_new ());
  tp_tests_contacts_connection_change_capabilities (f->service_conn,
      capabilities);
  g_hash_table_unref (capabilities);

  return contact;
}

static void
test_no_features (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpConnection *client_conn = f->client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL };
  const gchar * const ids[] = { "alice", "bob", "chris" };
  TpHandle handles[3] = { 0, 0, 0 };
  TpContact *contacts[3];
  guint i;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    contacts[i] = ensure_contact (f, ids[i], &handles[i]);

  tp_connection_upgrade_contacts_async (client_conn,
      3, contacts, NULL,
      upgrade_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  g_assert_cmpuint (result.contacts->len, ==, 3);
  g_assert (g_ptr_array_index (result.contacts, 0) == contacts[0]);
  g_assert (g_ptr_array_index (result.contacts, 1) == contacts[1]);
  g_assert (g_ptr_array_index (result.contacts, 2) == contacts[2]);

  reset_result (&result);

  for (i = 0; i < 3; i++)
    {
      MYASSERT (tp_contact_get_connection (contacts[i]) == client_conn, "");
      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==,
          ids[i]);
      g_assert_cmpstr (tp_contact_get_alias (contacts[i]), ==,
          tp_contact_get_identifier (contacts[i]));
      MYASSERT (tp_contact_get_avatar_token (contacts[i]) == NULL,
          ": %s", tp_contact_get_avatar_token (contacts[i]));
      g_assert_cmpuint (tp_contact_get_presence_type (contacts[i]), ==,
          TP_CONNECTION_PRESENCE_TYPE_UNSET);
      g_assert_cmpstr (tp_contact_get_presence_status (contacts[i]), ==,
          "");
      g_assert_cmpstr (tp_contact_get_presence_message (contacts[i]), ==,
          "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_LOCATION), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_CAPABILITIES), "");
    }

  for (i = 0; i < 3; i++)
    {
      g_object_unref (contacts[i]);
      tp_tests_proxy_run_until_dbus_queue_processed (client_conn);
    }

  /* remaining cleanup */
  g_main_loop_unref (result.loop);
}

/* Just put a country in locations for easier comparaisons.
 * FIXME: Ideally we should have a MYASSERT_SAME_ASV */
#define ASSERT_SAME_LOCATION(left_vardict, right)\
  G_STMT_START {\
    g_assert_cmpstr (g_variant_get_type_string (left_vardict), ==, "a{sv}"); \
    g_assert_cmpuint (g_variant_n_children (left_vardict), ==, \
        g_hash_table_size (right));\
    g_assert_cmpstr (tp_vardict_get_string (left_vardict, "country"), ==,\
        tp_asv_get_string (right, "country"));\
  } G_STMT_END

static void
free_rcc_list (GPtrArray *rccs)
{
  g_boxed_free (TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST, rccs);
}

static void
add_text_chat_class (GPtrArray *classes,
    TpEntityType entity_type)
{
  GHashTable *fixed;
  const gchar * const allowed[] = { NULL };
  GValueArray *arr;

  fixed = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, G_TYPE_UINT,
          entity_type,
      NULL);

  arr = tp_value_array_build (2,
      TP_HASH_TYPE_STRING_VARIANT_MAP, fixed,
      G_TYPE_STRV, allowed,
      G_TYPE_INVALID);

  g_hash_table_unref (fixed);

  g_ptr_array_add (classes, arr);
}

static GHashTable *
create_contact_caps (TpHandle *handles)
{
  GHashTable *capabilities;
  GPtrArray *caps1, *caps2, *caps3;

  capabilities = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) free_rcc_list);

  /* Support private text chats */
  caps1 = g_ptr_array_sized_new (2);
  add_text_chat_class (caps1, TP_ENTITY_TYPE_CONTACT);
  g_hash_table_insert (capabilities, GUINT_TO_POINTER (handles[0]), caps1);

  /* Support text chatrooms */
  caps2 = g_ptr_array_sized_new (1);
  add_text_chat_class (caps2, TP_ENTITY_TYPE_ROOM);
  g_hash_table_insert (capabilities, GUINT_TO_POINTER (handles[1]), caps2);

  /* Don't support anything */
  caps3 = g_ptr_array_sized_new (0);
  g_hash_table_insert (capabilities, GUINT_TO_POINTER (handles[2]), caps3);

  return capabilities;
}

static void
test_upgrade (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpTestsContactsConnection *service_conn = f->service_conn;
  TpConnection *client_conn = f->client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL };
  TpHandle handles[] = { 0, 0, 0 };
  static const gchar * const ids[] = { "alice", "bob", "chris" };
  static const gchar * const aliases[] = { "Alice in Wonderland",
      "Bob the Builder", "Christopher Robin" };
  static const gchar * const tokens[] = { "aaaaa", "bbbbb", "ccccc" };
  static TpTestsContactsConnectionPresenceStatusIndex statuses[] = {
      TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE,
      TP_TESTS_CONTACTS_CONNECTION_STATUS_BUSY,
      TP_TESTS_CONTACTS_CONNECTION_STATUS_AWAY };
  static const gchar * const messages[] = { "", "Fixing it",
      "GON OUT BACKSON" };
  GHashTable *location_1 = tp_asv_new (
      "country",  G_TYPE_STRING, "United Kingdom of Great Britain and Northern Ireland", NULL);
  GHashTable *location_2 = tp_asv_new (
      "country",  G_TYPE_STRING, "Atlantis", NULL);
  GHashTable *location_3 = tp_asv_new (
      "country",  G_TYPE_STRING, "Belgium", NULL);
  GHashTable *locations[] = { location_1, location_2, location_3 };
  GHashTable *capabilities;
  TpContact *contacts[3];
  GQuark features[] = { TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN, TP_CONTACT_FEATURE_PRESENCE,
      TP_CONTACT_FEATURE_LOCATION, TP_CONTACT_FEATURE_CAPABILITIES, 0 };
  guint i;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    contacts[i] = ensure_contact (f, ids[i], &handles[i]);

  tp_tests_contacts_connection_change_aliases (service_conn, 3, handles,
      aliases);
  tp_tests_contacts_connection_change_presences (service_conn, 3, handles,
      statuses, messages);
  tp_tests_contacts_connection_change_avatar_tokens (service_conn, 3, handles,
      tokens);
  tp_tests_contacts_connection_change_locations (service_conn, 3, handles,
      locations);

  capabilities = create_contact_caps (handles);
  tp_tests_contacts_connection_change_capabilities (service_conn, capabilities);
  g_hash_table_unref (capabilities);

  tp_connection_upgrade_contacts_async (client_conn,
      3, contacts, NULL,
      upgrade_cb, &result);

  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  g_assert_cmpuint (result.contacts->len, ==, 3);
  g_assert (g_ptr_array_index (result.contacts, 0) == contacts[0]);
  g_assert (g_ptr_array_index (result.contacts, 1) == contacts[1]);
  g_assert (g_ptr_array_index (result.contacts, 2) == contacts[2]);

  for (i = 0; i < 3; i++)
    {
      MYASSERT (tp_contact_get_connection (contacts[i]) == client_conn, "");
      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==,
          ids[i]);
      g_assert_cmpstr (tp_contact_get_alias (contacts[i]), ==,
          tp_contact_get_identifier (contacts[i]));
      MYASSERT (tp_contact_get_avatar_token (contacts[i]) == NULL,
          ": %s", tp_contact_get_avatar_token (contacts[i]));
      g_assert_cmpuint (tp_contact_get_presence_type (contacts[i]), ==,
          TP_CONNECTION_PRESENCE_TYPE_UNSET);
      g_assert_cmpstr (tp_contact_get_presence_status (contacts[i]), ==,
          "");
      g_assert_cmpstr (tp_contact_get_presence_message (contacts[i]), ==,
          "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_LOCATION), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_CAPABILITIES), "");
    }

  /* clean up before doing the second request */
  g_assert (result.error == NULL);
  reset_result (&result);

  tp_connection_upgrade_contacts_async (client_conn,
      3, contacts, features,
      upgrade_cb, &result);

  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  g_assert_cmpuint (result.contacts->len, ==, 3);
  g_assert (g_ptr_array_index (result.contacts, 0) == contacts[0]);
  g_assert (g_ptr_array_index (result.contacts, 1) == contacts[1]);
  g_assert (g_ptr_array_index (result.contacts, 2) == contacts[2]);

  reset_result (&result);

  for (i = 0; i < 3; i++)
    {
      GVariant *vardict;

      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==,
          ids[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      g_assert_cmpstr (tp_contact_get_alias (contacts[i]), ==,
          aliases[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      g_assert_cmpstr (tp_contact_get_avatar_token (contacts[i]), ==,
          tokens[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      g_assert_cmpstr (tp_contact_get_presence_message (contacts[i]), ==,
          messages[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_LOCATION), "");

      vardict = tp_contact_dup_location (contacts[i]);
      ASSERT_SAME_LOCATION (vardict, locations[i]);
      g_variant_unref (vardict);

      g_object_get (contacts[i],
          "location", &vardict,
          NULL);
      ASSERT_SAME_LOCATION (vardict, locations[i]);
      g_variant_unref (vardict);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_CAPABILITIES), "");
      MYASSERT (tp_contact_get_capabilities (contacts[i]) != NULL, "");
    }

  g_assert_cmpuint (tp_contact_get_presence_type (contacts[0]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[0]), ==,
      "available");
  g_assert_cmpuint (tp_contact_get_presence_type (contacts[1]), ==,
      TP_CONNECTION_PRESENCE_TYPE_BUSY);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[1]), ==,
      "busy");
  g_assert_cmpuint (tp_contact_get_presence_type (contacts[2]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[2]), ==,
      "away");

  for (i = 0; i < 3; i++)
    {
      g_object_unref (contacts[i]);
      tp_tests_proxy_run_until_dbus_queue_processed (client_conn);
    }

  /* remaining cleanup */
  g_hash_table_unref (location_1);
  g_hash_table_unref (location_2);
  g_hash_table_unref (location_3);
  g_main_loop_unref (result.loop);
}

/* Regression test case for fd.o#41414 */
static void
test_upgrade_noop (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL };
  TpHandle handle;
  TpContact *contact;

  g_test_bug ("41414");

  /* Get a contact by handle */
  contact = ensure_contact_no_caps (f, "test-upgrade-noop", &handle);
  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &contact, (const GQuark *) f->all_contact_features->data,
      upgrade_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  reset_result (&result);

  /* Upgrade it again, but it should already have all features */
  make_the_connection_disappear (f);
  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &contact, (const GQuark *) f->all_contact_features->data,
      upgrade_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);
  reset_result (&result);

  put_the_connection_back (f);

  g_object_unref (contact);
}

typedef struct
{
  gboolean alias_changed;
  gboolean avatar_token_changed;
  gboolean presence_type_changed;
  gboolean presence_status_changed;
  gboolean presence_msg_changed;
  gboolean location_changed;
  gboolean capabilities_changed;
} notify_ctx;

static void
notify_ctx_init (notify_ctx *ctx)
{
  ctx->alias_changed = FALSE;
  ctx->avatar_token_changed = FALSE;
  ctx->presence_type_changed = FALSE;
  ctx->presence_status_changed = FALSE;
  ctx->presence_msg_changed = FALSE;
  ctx->location_changed = FALSE;
  ctx->capabilities_changed = FALSE;
}

static gboolean
notify_ctx_is_fully_changed (notify_ctx *ctx)
{
  return ctx->alias_changed && ctx->avatar_token_changed &&
    ctx->presence_type_changed && ctx->presence_status_changed &&
    ctx->presence_msg_changed && ctx->location_changed &&
    ctx->capabilities_changed;
}

static gboolean
notify_ctx_is_changed (notify_ctx *ctx)
{
  return ctx->alias_changed || ctx->avatar_token_changed ||
    ctx->presence_type_changed || ctx->presence_status_changed ||
    ctx->presence_msg_changed || ctx->location_changed ||
    ctx->capabilities_changed;
}

static void
contact_notify_cb (TpContact *contact,
    GParamSpec *param,
    notify_ctx *ctx)
{
  if (!tp_strdiff (param->name, "alias"))
    ctx->alias_changed = TRUE;
  else if (!tp_strdiff (param->name, "avatar-token"))
    ctx->avatar_token_changed = TRUE;
  else if (!tp_strdiff (param->name, "presence-type"))
    ctx->presence_type_changed = TRUE;
  else if (!tp_strdiff (param->name, "presence-status"))
    ctx->presence_status_changed = TRUE;
  else if (!tp_strdiff (param->name, "presence-message"))
    ctx->presence_msg_changed = TRUE;
  else if (!tp_strdiff (param->name, "location"))
    ctx->location_changed = TRUE;
  else if (!tp_strdiff (param->name, "capabilities"))
    ctx->capabilities_changed = TRUE;
}

static GHashTable *
create_new_contact_caps (TpHandle *handles)
{
  GHashTable *capabilities;
  GPtrArray *caps1, *caps2;

  capabilities = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) free_rcc_list);

  /* Support private text chats and chatrooms */
  caps1 = g_ptr_array_sized_new (2);
  add_text_chat_class (caps1, TP_ENTITY_TYPE_CONTACT);
  add_text_chat_class (caps1, TP_ENTITY_TYPE_ROOM);
  g_hash_table_insert (capabilities, GUINT_TO_POINTER (handles[0]), caps1);

  /* Don't support anything */
  caps2 = g_ptr_array_sized_new (0);
  g_hash_table_insert (capabilities, GUINT_TO_POINTER (handles[1]), caps2);

  return capabilities;
}

static void
test_features (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpTestsContactsConnection *service_conn = f->service_conn;
  TpConnection *client_conn = f->client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL };
  TpHandle handles[] = { 0, 0, 0 };
  static const gchar * const ids[] = { "alice", "bob", "chris" };
  static const gchar * const aliases[] = { "Alice in Wonderland",
      "Bob the Builder", "Christopher Robin" };
  static const gchar * const tokens[] = { "aaaaa", "bbbbb", "ccccc" };
  static TpTestsContactsConnectionPresenceStatusIndex statuses[] = {
      TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE,
      TP_TESTS_CONTACTS_CONNECTION_STATUS_BUSY,
      TP_TESTS_CONTACTS_CONNECTION_STATUS_AWAY };
  static const gchar * const messages[] = { "", "Fixing it",
      "GON OUT BACKSON" };
  static const gchar * const new_aliases[] = { "Alice [at a tea party]",
      "Bob the Plumber" };
  static const gchar * const new_tokens[] = { "AAAA", "BBBB" };
  static TpTestsContactsConnectionPresenceStatusIndex new_statuses[] = {
      TP_TESTS_CONTACTS_CONNECTION_STATUS_AWAY,
      TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE };
  static const gchar * const new_messages[] = { "At the Mad Hatter's",
      "It'll cost you" };
  GHashTable *location_1 = tp_asv_new (
      "country",  G_TYPE_STRING, "United Kingdom of Great Britain and Northern Ireland", NULL);
  GHashTable *location_2 = tp_asv_new (
      "country",  G_TYPE_STRING, "Atlantis", NULL);
  GHashTable *location_3 = tp_asv_new (
      "country",  G_TYPE_STRING, "Belgium", NULL);
  GHashTable *locations[] = { location_1, location_2, location_3 };
  GHashTable *location_4 = tp_asv_new (
      "country",  G_TYPE_STRING, "France", NULL);
  GHashTable *location_5 = tp_asv_new (
      "country",  G_TYPE_STRING, "Éire", NULL);
  GHashTable *new_locations[] = { location_4, location_5 };
  GHashTable *capabilities, *new_capabilities;
  gboolean support_text_chats[] = { TRUE, FALSE, FALSE };
  gboolean support_text_chatrooms[] = { FALSE, TRUE, FALSE };
  gboolean new_support_text_chats[] = { TRUE, FALSE };
  gboolean new_support_text_chatrooms[] = { TRUE, FALSE };
  TpContact *contacts[3];
  GQuark features[] = { TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN, TP_CONTACT_FEATURE_PRESENCE,
      TP_CONTACT_FEATURE_LOCATION, TP_CONTACT_FEATURE_CAPABILITIES, 0 };
  guint i;
  struct {
      TpConnection *connection;
      TpHandle handle;
      gchar *identifier;
      gchar *alias;
      gchar *avatar_token;
      TpConnectionPresenceType presence_type;
      gchar *presence_status;
      gchar *presence_message;
      GVariant *location_vardict;
      TpCapabilities *capabilities;
  } from_gobject;
  notify_ctx notify_ctx_alice, notify_ctx_chris;
  GVariant *vardict;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    contacts[i] = ensure_contact (f, ids[i], &handles[i]);

  tp_tests_contacts_connection_change_aliases (service_conn, 3, handles,
      aliases);
  tp_tests_contacts_connection_change_presences (service_conn, 3, handles,
      statuses, messages);
  tp_tests_contacts_connection_change_avatar_tokens (service_conn, 3, handles,
      tokens);
  tp_tests_contacts_connection_change_locations (service_conn, 3, handles,
      locations);

  /* contact capabilities */
  capabilities = create_contact_caps (handles);
  tp_tests_contacts_connection_change_capabilities (service_conn,
      capabilities);
  g_hash_table_unref (capabilities);

  tp_connection_upgrade_contacts_async (client_conn,
      3, contacts, features,
      upgrade_cb, &result);

  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  g_assert_cmpuint (result.contacts->len, ==, 3);
  g_assert (g_ptr_array_index (result.contacts, 0) == contacts[0]);
  g_assert (g_ptr_array_index (result.contacts, 1) == contacts[1]);
  g_assert (g_ptr_array_index (result.contacts, 2) == contacts[2]);

  g_assert (result.error == NULL);
  reset_result (&result);

  for (i = 0; i < 3; i++)
    {
      TpCapabilities *caps;

      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==,
          ids[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      g_assert_cmpstr (tp_contact_get_alias (contacts[i]), ==,
          aliases[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      g_assert_cmpstr (tp_contact_get_avatar_token (contacts[i]), ==,
          tokens[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      g_assert_cmpstr (tp_contact_get_presence_message (contacts[i]), ==,
          messages[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_LOCATION), "");
      vardict = tp_contact_dup_location (contacts[i]);
      ASSERT_SAME_LOCATION (vardict, locations[i]);
      g_variant_unref (vardict);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_CAPABILITIES), "");

      caps = tp_contact_get_capabilities (contacts[i]);
      MYASSERT (caps != NULL, "");
      MYASSERT (tp_capabilities_is_specific_to_contact (caps), "");
      MYASSERT (tp_capabilities_supports_text_chats (caps) ==
          support_text_chats[i], " contact %u", i);
      MYASSERT (tp_capabilities_supports_text_chatrooms (caps) ==
          support_text_chatrooms[i], " contact %u", i);
    }

  g_assert_cmpuint (tp_contact_get_presence_type (contacts[0]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[0]), ==,
      "available");
  g_assert_cmpuint (tp_contact_get_presence_type (contacts[1]), ==,

      TP_CONNECTION_PRESENCE_TYPE_BUSY);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[1]), ==,
      "busy");
  g_assert_cmpuint (tp_contact_get_presence_type (contacts[2]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[2]), ==,
      "away");

  /* exercise GObject properties in a basic way */
  g_object_get (contacts[0],
      "connection", &from_gobject.connection,
      "handle", &from_gobject.handle,
      "identifier", &from_gobject.identifier,
      "alias", &from_gobject.alias,
      "avatar-token", &from_gobject.avatar_token,
      "presence-type", &from_gobject.presence_type,
      "presence-status", &from_gobject.presence_status,
      "presence-message", &from_gobject.presence_message,
      "location", &from_gobject.location_vardict,
      "capabilities", &from_gobject.capabilities,
      NULL);
  MYASSERT (from_gobject.connection == client_conn, "");
  g_assert_cmpuint (from_gobject.handle, ==, handles[0]);
  g_assert_cmpstr (from_gobject.identifier, ==, "alice");
  g_assert_cmpstr (from_gobject.alias, ==, "Alice in Wonderland");
  g_assert_cmpstr (from_gobject.avatar_token, ==, "aaaaa");
  g_assert_cmpuint (from_gobject.presence_type, ==,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (from_gobject.presence_status, ==, "available");
  g_assert_cmpstr (from_gobject.presence_message, ==, "");
  ASSERT_SAME_LOCATION (from_gobject.location_vardict,
      locations[0]);
  MYASSERT (tp_capabilities_is_specific_to_contact (from_gobject.capabilities),
      "");
  MYASSERT (tp_capabilities_supports_text_chats (from_gobject.capabilities)
      == support_text_chats[0], "");
  MYASSERT (tp_capabilities_supports_text_chatrooms (from_gobject.capabilities)
      == support_text_chatrooms[0], "");
  g_object_unref (from_gobject.connection);
  g_free (from_gobject.identifier);
  g_free (from_gobject.alias);
  g_free (from_gobject.avatar_token);
  g_free (from_gobject.presence_status);
  g_free (from_gobject.presence_message);
  g_variant_unref (from_gobject.location_vardict);
  g_object_unref (from_gobject.capabilities);

  notify_ctx_init (&notify_ctx_alice);
  g_signal_connect (contacts[0], "notify",
      G_CALLBACK (contact_notify_cb), &notify_ctx_alice);

  notify_ctx_init (&notify_ctx_chris);
  g_signal_connect (contacts[2], "notify",
      G_CALLBACK (contact_notify_cb), &notify_ctx_chris);

  /* Change Alice and Bob's contact info, leave Chris as-is */
  tp_tests_contacts_connection_change_aliases (service_conn, 2, handles,
      new_aliases);
  tp_tests_contacts_connection_change_presences (service_conn, 2, handles,
      new_statuses, new_messages);
  tp_tests_contacts_connection_change_avatar_tokens (service_conn, 2, handles,
      new_tokens);
  tp_tests_contacts_connection_change_locations (service_conn, 2, handles,
      new_locations);

  new_capabilities = create_new_contact_caps (handles);
  tp_tests_contacts_connection_change_capabilities (service_conn,
      new_capabilities);
  g_hash_table_unref (new_capabilities);

  tp_tests_proxy_run_until_dbus_queue_processed (client_conn);

  g_assert (notify_ctx_is_fully_changed (&notify_ctx_alice));
  g_assert (!notify_ctx_is_changed (&notify_ctx_chris));

  for (i = 0; i < 2; i++)
    {
      TpCapabilities *caps;

      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==,
          ids[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      g_assert_cmpstr (tp_contact_get_alias (contacts[i]), ==,
          new_aliases[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      g_assert_cmpstr (tp_contact_get_avatar_token (contacts[i]), ==,
          new_tokens[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      g_assert_cmpstr (tp_contact_get_presence_message (contacts[i]), ==,
          new_messages[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_LOCATION), "");
      vardict = tp_contact_dup_location (contacts[i]);
      ASSERT_SAME_LOCATION (vardict, new_locations[i]);
      g_variant_unref (vardict);

      caps = tp_contact_get_capabilities (contacts[i]);
      MYASSERT (caps != NULL, "");
      MYASSERT (tp_capabilities_is_specific_to_contact (caps), "");
      MYASSERT (tp_capabilities_supports_text_chats (caps) ==
          new_support_text_chats[i], " contact %u", i);
      MYASSERT (tp_capabilities_supports_text_chatrooms (caps) ==
          new_support_text_chatrooms[i], " contact %u", i);
    }

  g_assert_cmpuint (tp_contact_get_presence_type (contacts[0]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[0]), ==,
      "away");
  g_assert_cmpuint (tp_contact_get_presence_type (contacts[1]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[1]), ==,
      "available");

  for (i = 0; i < 3; i++)
    {
      g_object_unref (contacts[i]);
      tp_tests_proxy_run_until_dbus_queue_processed (client_conn);
    }

  /* remaining cleanup */
  g_main_loop_unref (result.loop);
  g_hash_table_unref (location_1);
  g_hash_table_unref (location_2);
  g_hash_table_unref (location_3);
  g_hash_table_unref (location_4);
  g_hash_table_unref (location_5);
}

static void
by_id_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpConnection *connection = (TpConnection *) source;
  Result *r = user_data;
  TpContact *contact;

  g_assert (r->contacts == NULL);
  g_assert (r->error == NULL);

  r->contacts = g_ptr_array_new_with_free_func (g_object_unref);

  contact = tp_connection_dup_contact_by_id_finish (connection, result, &r->error);
  if (contact != NULL)
    g_ptr_array_add (r->contacts, contact);

  g_main_loop_quit (r->loop);
}

static void
test_by_id (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpConnection *client_conn = f->client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL };
  TpContact *contact;

  g_message ("%s: all bad (fd.o #19688)", G_STRFUNC);

  tp_connection_dup_contact_by_id_async (client_conn,
      "Not valid",
      NULL,
      by_id_cb,
      &result);

  g_main_loop_run (result.loop);

  g_assert_cmpuint (result.contacts->len, ==, 0);
  g_assert_error (result.error, TP_ERROR, TP_ERROR_INVALID_HANDLE);

  reset_result (&result);

  g_message ("%s: all good", G_STRFUNC);

  tp_connection_dup_contact_by_id_async (client_conn,
      "Alice",
      NULL,
      by_id_cb,
      &result);

  g_main_loop_run (result.loop);

  g_assert_cmpuint (result.contacts->len, ==, 1);
  g_assert_no_error (result.error);

  contact = g_ptr_array_index (result.contacts, 0);
  g_assert (contact != NULL);
  g_assert_cmpstr (tp_contact_get_identifier (contact), ==, "alice");

  reset_result (&result);
}


static void
test_dup_if_possible (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle alice_handle, bob_handle;
  TpContact *alice;
  TpContact *contact;

  alice_handle = tp_handle_ensure (f->service_repo, "alice", NULL, NULL);
  g_assert_cmpuint (alice_handle, !=, 0);
  bob_handle = tp_handle_ensure (f->service_repo, "bob", NULL, NULL);
  g_assert_cmpuint (bob_handle, !=, 0);

  tp_connection_dup_contact_by_id_async (f->client_conn,
      "alice",
      NULL,
      by_id_cb,
      &f->result);
  g_main_loop_run (f->result.loop);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);
  g_assert_no_error (f->result.error);

  g_assert (g_ptr_array_index (f->result.contacts, 0) != NULL);
  alice = g_object_ref (g_ptr_array_index (f->result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (alice), ==, alice_handle);
  g_assert_cmpstr (tp_contact_get_identifier (alice), ==, "alice");

  reset_result (&f->result);

  /* we already have a cached TpContact for Alice, so we can get another
   * copy of it synchronously */

  contact = tp_connection_dup_contact_if_possible (f->client_conn,
      alice_handle, "alice");
  g_assert (contact == alice);
  g_object_unref (contact);

  contact = tp_connection_dup_contact_if_possible (f->client_conn,
      alice_handle, NULL);
  g_assert (contact == alice);
  g_object_unref (contact);

  /* because this connection has immortal handles, we can reliably get a
   * contact for Bob synchronously, but only if we supply his identifier */

  contact = tp_connection_dup_contact_if_possible (f->client_conn,
      bob_handle, NULL);
  g_assert (contact == NULL);

  contact = tp_connection_dup_contact_if_possible (f->client_conn,
      bob_handle, "bob");
  g_assert (contact != alice);
  g_assert_cmpstr (tp_contact_get_identifier (contact), ==, "bob");
  g_assert_cmpuint (tp_contact_get_handle (contact), ==, bob_handle);
  g_object_unref (contact);
  g_object_unref (alice);
}

typedef struct
{
  gboolean signal_received;
  TpSubscriptionState subscribe;
  TpSubscriptionState publish;
  gchar *publish_request;
} SubscriptionStates;

static void
subscription_states_changed_cb (TpContact *contact,
    TpSubscriptionState subscribe,
    TpSubscriptionState publish,
    const gchar *publish_request,
    SubscriptionStates *states)
{
  g_assert_cmpint (tp_contact_get_subscribe_state (contact), ==, subscribe);
  g_assert_cmpint (tp_contact_get_publish_state (contact), ==, publish);
  g_assert_cmpstr (tp_contact_get_publish_request (contact), ==,
      publish_request);

  states->signal_received = TRUE;
  states->subscribe = subscribe;
  states->publish = publish;
  g_clear_pointer (&states->publish_request, g_free);
  states->publish_request = g_strdup (publish_request);
}

static void
test_subscription_states (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle alice_handle;
  TpContact *alice;
  TpTestsContactListManager *manager;
  GQuark features[] = { TP_CONTACT_FEATURE_SUBSCRIPTION_STATES, 0 };
  SubscriptionStates states = { FALSE, TP_SUBSCRIPTION_STATE_NO,
      TP_SUBSCRIPTION_STATE_NO, NULL };

  manager = tp_tests_contacts_connection_get_contact_list_manager (
      f->service_conn);

  alice = ensure_contact (f, "alice", &alice_handle);
  g_assert_cmpuint (alice_handle, !=, 0);

  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &alice, features,
      upgrade_cb, &f->result);
  g_main_loop_run (f->result.loop);
  g_assert_no_error (f->result.error);

  g_assert_cmpint (tp_contact_get_subscribe_state (alice), ==,
      TP_SUBSCRIPTION_STATE_NO);
  g_assert_cmpint (tp_contact_get_publish_state (alice), ==,
      TP_SUBSCRIPTION_STATE_NO);
  g_assert_cmpstr (tp_contact_get_publish_request (alice), ==,
      "");

  reset_result (&f->result);

  g_signal_connect (alice, "subscription-states-changed",
      G_CALLBACK (subscription_states_changed_cb), &states);

  /* Request subscription */
  tp_tests_contact_list_manager_request_subscription (manager, 1, &alice_handle, "");

  while (!states.signal_received)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpint (states.subscribe, ==, TP_SUBSCRIPTION_STATE_ASK);
  g_assert_cmpint (states.publish, ==, TP_SUBSCRIPTION_STATE_NO);
  g_assert_cmpstr (states.publish_request, ==, "");
  g_clear_pointer (&states.publish_request, g_free);
  states.signal_received = FALSE;

  /* Request again must re-emit the signal. Saying please this time will make
   * the request accepted and will ask for publish. */
  tp_tests_contact_list_manager_request_subscription (manager, 1, &alice_handle, "please");

  while (!states.signal_received)
    g_main_context_iteration (NULL, TRUE);

  if (states.subscribe != TP_SUBSCRIPTION_STATE_YES)
    {
      /* we might receive this repeated signal in the same main loop
       * iteration as the YES/ASK/"automatic publish request" one below,
       * in which case it isn't visible to this regression test,
       * or we might receive it in its own main loop iteration */
      g_assert_cmpint (states.subscribe, ==, TP_SUBSCRIPTION_STATE_ASK);
      g_assert_cmpint (states.publish, ==, TP_SUBSCRIPTION_STATE_NO);
      g_assert_cmpstr (states.publish_request, ==, "");
      g_clear_pointer (&states.publish_request, g_free);
      states.signal_received = FALSE;

      while (!states.signal_received)
        g_main_context_iteration (NULL, TRUE);
    }

  g_assert_cmpint (states.subscribe, ==, TP_SUBSCRIPTION_STATE_YES);
  g_assert_cmpint (states.publish, ==, TP_SUBSCRIPTION_STATE_ASK);
  g_assert_cmpstr (states.publish_request, ==, "automatic publish request");
  g_clear_pointer (&states.publish_request, g_free);
  states.signal_received = FALSE;

  /* Remove the contact */
  tp_tests_contact_list_manager_remove (manager, 1, &alice_handle);

  while (!states.signal_received)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpint (states.subscribe, ==, TP_SUBSCRIPTION_STATE_NO);
  g_assert_cmpint (states.publish, ==, TP_SUBSCRIPTION_STATE_NO);
  g_assert_cmpstr (states.publish_request, ==, "");
  g_clear_pointer (&states.publish_request, g_free);
  states.signal_received = FALSE;

  g_object_unref (alice);
}

typedef struct
{
  GPtrArray *groups;
  GMainLoop *loop;
} ContactGroups;

static void
assert_contact_groups (TpContact *contact,
    ContactGroups *data)
{
  const gchar * const *groups = tp_contact_get_contact_groups (contact);
  guint i;

  g_assert (groups != NULL);
  g_assert_cmpuint (g_strv_length ((GStrv) groups), ==, data->groups->len);

  for (i = 0; i < data->groups->len; i++)
    g_assert (tp_strv_contains (groups, g_ptr_array_index (data->groups, i)));
}

static void
contact_groups_changed_cb (TpContact *contact,
    GStrv added,
    GStrv removed,
    ContactGroups *data)
{
  assert_contact_groups (contact, data);
  g_main_loop_quit (data->loop);
}

static void
test_contact_groups (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle alice_handle;
  TpContact *alice;
  TpTestsContactListManager *manager;
  GQuark features[] = { TP_CONTACT_FEATURE_CONTACT_GROUPS, 0 };
  ContactGroups data;

  data.groups = g_ptr_array_new ();
  data.loop = f->result.loop;

  manager = tp_tests_contacts_connection_get_contact_list_manager (
      f->service_conn);

  alice = ensure_contact (f, "alice", &alice_handle);
  g_assert_cmpuint (alice_handle, !=, 0);

  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &alice, features,
      upgrade_cb, &f->result);
  g_main_loop_run (f->result.loop);
  g_assert_no_error (f->result.error);

  assert_contact_groups (alice, &data);

  reset_result (&f->result);

  g_signal_connect (alice, "contact-groups-changed",
      G_CALLBACK (contact_groups_changed_cb), &data);

  g_ptr_array_add (data.groups, "group1");
  tp_tests_contact_list_manager_add_to_group (manager, "group1", alice_handle);
  g_main_loop_run (data.loop);

  g_ptr_array_add (data.groups, "group2");
  tp_tests_contact_list_manager_add_to_group (manager, "group2", alice_handle);
  g_main_loop_run (data.loop);

  g_ptr_array_remove_index_fast (data.groups, 0);
  tp_tests_contact_list_manager_remove_from_group (manager, "group1", alice_handle);
  g_main_loop_run (data.loop);

  g_ptr_array_set_size (data.groups, 0);
  g_ptr_array_add (data.groups, "group1");
  g_ptr_array_add (data.groups, "group2");
  g_ptr_array_add (data.groups, "group3");
  tp_contact_set_contact_groups_async (alice, data.groups->len,
      (const gchar * const *) data.groups->pdata, NULL, NULL);
  g_main_loop_run (data.loop);

  g_ptr_array_unref (data.groups);
  g_object_unref (alice);
}

static void
assert_no_location (TpContact *contact)
{
  /* We could reasonably represent “no published location” as NULL or as an
   * empty a{sv}, so allow both.
   */
  GVariant *retrieved_location = tp_contact_dup_location (contact);

  if (retrieved_location != NULL)
    {
      g_assert (g_variant_n_children (retrieved_location) == 0);
      g_variant_unref (retrieved_location);
    }
}

/* This is a regression test for an issue where the LOCATION feature would
 * never be marked as prepared for contacts with no published location, so
 * repeated calls to tp_connection_upgrade_contacts_async() would call
 * GetContactAttributes() over and over. It's really a special case of
 * test_upgrade_noop(), but presented separately for clarity.
 */
static void
test_no_location (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle handle;
  TpContact *contact;
  gpointer weak_pointer;
  GQuark features[] = { TP_CONTACT_FEATURE_LOCATION, 0 };
  GHashTable *norway = tp_asv_new ("country",  G_TYPE_STRING, "Norway", NULL);
  notify_ctx notify_ctx_alice;
  GVariant *vardict;

  g_test_bug ("39377");

  contact = ensure_contact (f, "alice", &handle);
  g_assert_cmpuint (handle, !=, 0);

  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &contact, features,
      upgrade_cb, &f->result);
  g_main_loop_run (f->result.loop);
  g_assert_no_error (f->result.error);

  assert_no_location (contact);
  reset_result (&f->result);

  /* Although Alice doesn't have a published location, the feature's still been
   * prepared, so we shouldn't need any D-Bus traffic to re-fetch her TpContact.
   */
  make_the_connection_disappear (f);
  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &contact, features,
      upgrade_cb, &f->result);
  g_main_loop_run (f->result.loop);
  g_assert_no_error (f->result.error);

  assert_no_location (contact);

  put_the_connection_back (f);
  g_assert (f->result.error == NULL);
  reset_result (&f->result);

  /* Despite Alice not currently having a published location, we should
   * certainly be listening to changes to her location.
   */
  notify_ctx_init (&notify_ctx_alice);
  g_signal_connect (contact, "notify",
      G_CALLBACK (contact_notify_cb), &notify_ctx_alice);

  tp_tests_contacts_connection_change_locations (f->service_conn,
      1, &handle, &norway);
  tp_tests_proxy_run_until_dbus_queue_processed (f->client_conn);
  g_assert (notify_ctx_alice.location_changed);
  vardict = tp_contact_dup_location (contact);
  ASSERT_SAME_LOCATION (vardict, norway);
  g_variant_unref (vardict);

  weak_pointer = contact;
  g_object_add_weak_pointer ((GObject *) contact, &weak_pointer);
  g_object_unref (contact);
  g_assert (weak_pointer == NULL);

  /* Check that first retrieving a contact without the LOCATION feature, and
   * later upgrading it to have the LOCATION feature, does the right thing.
   * As with "TEST 4" in the location tests, we must defer creating the
   * contact until after we have changed its location at the service side. */
  handle = ensure_handle (f, "rupert");
  g_assert_cmpuint (handle, !=, 0);

  tp_tests_contacts_connection_change_locations (f->service_conn,
      1, &handle, &norway);
  tp_tests_proxy_run_until_dbus_queue_processed (f->client_conn);

  contact = tp_connection_dup_contact_if_possible (f->client_conn, handle,
      "rupert");

  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &contact, NULL,
      upgrade_cb, &f->result);
  g_main_loop_run (f->result.loop);
  g_assert_no_error (f->result.error);

  assert_no_location (contact);

  /* clean up before doing the second request */
  reset_result (&f->result);

  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &contact, features,
      upgrade_cb, &f->result);
  g_main_loop_run (f->result.loop);
  g_assert_no_error (f->result.error);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);

  g_assert (g_ptr_array_index (f->result.contacts, 0) == contact);
  vardict = tp_contact_dup_location (contact);
  ASSERT_SAME_LOCATION (vardict, norway);
  g_variant_unref (vardict);
  reset_result (&f->result);

  weak_pointer = contact;
  g_object_add_weak_pointer ((GObject *) contact, &weak_pointer);
  g_object_unref (contact);
  g_assert (weak_pointer == NULL);

  tp_tests_proxy_run_until_dbus_queue_processed (f->client_conn);
}

static void
setup_broken_client_types_conn (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  tp_tests_create_and_connect_conn (
      TP_TESTS_TYPE_BROKEN_CLIENT_TYPES_CONNECTION,
      "me@test.com", &f->base_connection, &f->client_conn);

  f->service_conn = TP_TESTS_CONTACTS_CONNECTION (f->base_connection);
  g_object_ref (f->service_conn);

  f->service_repo = tp_base_connection_get_handles (f->base_connection,
      TP_ENTITY_TYPE_CONTACT);
  f->result.loop = g_main_loop_new (NULL, FALSE);
}

static void
test_superfluous_attributes (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle handle;
  TpContact *contact;
  const gchar * const *client_types;
  GQuark client_types_features[] = { TP_CONTACT_FEATURE_CLIENT_TYPES, 0 };
  GQuark presence_features[] = { TP_CONTACT_FEATURE_PRESENCE, 0 };

  g_assert (TP_TESTS_IS_BROKEN_CLIENT_TYPES_CONNECTION (f->service_conn));

  contact = ensure_contact (f, "helge", &handle);
  g_assert_cmpuint (handle, !=, 0);

  /* We ask for ClientTypes; the CM is broken and adds Presence
   * information to the reply... it also omits the /client-types attribute from
   * the reply, which, since the spec says “Omitted from the result if the
   * contact's client types are not known.” leaves us in the exciting position
   * of having to decide between marking the feature as prepared anyway or
   * saying it failed, and also deciding whether get_client_types returns [] or
   * NULL...
   */
  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &contact, client_types_features,
      upgrade_cb, &f->result);
  g_main_loop_run (f->result.loop);
  g_assert_no_error (f->result.error);

  /* She doesn't have any client types. There are two reasonable ways to
   * represent this.
   */
  client_types = tp_contact_get_client_types (contact);
  if (client_types != NULL)
    g_assert_cmpstr (client_types[0], ==, NULL);

  /* She also shouldn't have any presence information, despite it being
   * inexplicably included in the GetContactAttributes reply. Specifically:
   * because we have not connected to PresencesChanged, it's not safe to just
   * randomly stash this information and mark the feature as prepared.
   *
   * (If we wanted to be really smart we could do something like: if the
   * information's there for some reason, and we happen already to be bound to
   * PresencesChanged due to preparing that feature on another contact … then
   * accept the mysterious information. But that seems fragile and prone to
   * people relying on sketchy behaviour.)
   */
  g_assert_cmpstr (tp_contact_get_presence_message (contact), ==, "");
  g_assert_cmpstr (tp_contact_get_presence_status (contact), ==, "");
  g_assert_cmpuint (tp_contact_get_presence_type (contact), ==,
      TP_CONNECTION_PRESENCE_TYPE_UNSET);

  reset_result (&f->result);

  /* So now if we try to prepare TP_CONTACT_FEATURE_PRESENCE, we should need to
   * make some D-Bus calls: it shouldn't have been marked prepared by the
   * previous call. Successfully upgrading to this feature is tested
   * elsewhere, so we'll test that upgrading fails if the connection's
   * mysteriously died.
   */
  make_the_connection_disappear (f);
  tp_connection_upgrade_contacts_async (f->client_conn,
      1, &contact, presence_features,
      upgrade_cb, &f->result);
  g_main_loop_run (f->result.loop);
  /* Not gonna make any particular assertions about what the error is. */
  g_assert_error (f->result.error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);

  put_the_connection_back (f);
  reset_result (&f->result);
  g_object_unref (contact);
}

static void
contact_list_changed_cb (TpConnection *connection,
    GPtrArray *added,
    GPtrArray *removed,
    gpointer user_data)
{
  gboolean *received = user_data;

  g_assert (added != NULL);
  g_assert (removed != NULL);

  *received = TRUE;
}

static void
test_contact_list (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  const GQuark conn_features[] = { TP_CONNECTION_FEATURE_CONTACT_LIST, 0 };
  const GQuark feature_connected[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };
  Result result = { g_main_loop_new (NULL, FALSE), NULL };
  TpTestsContactListManager *manager;
  TpClientFactory *factory;
  const gchar *id = "contact-list-id";
  const gchar *alias = "Contact List Alias";
  const gchar *message = "I'm your best friend";
  TpHandle handle;
  GPtrArray *contacts;
  TpContact *contact;
  gboolean got_contact_list_changed = FALSE;

  manager = tp_tests_contacts_connection_get_contact_list_manager (
      f->service_conn);

  /* Connection is OFFLINE initially */
  tp_tests_proxy_run_until_prepared (f->client_conn, conn_features);
  g_assert_cmpint (tp_connection_get_contact_list_state (f->client_conn), ==,
      TP_CONTACT_LIST_STATE_NONE);
  g_assert (tp_connection_get_contact_list_persists (f->client_conn));
  g_assert (tp_connection_get_can_change_contact_list (f->client_conn));
  g_assert (tp_connection_get_request_uses_message (f->client_conn));

  /* Add a remote-pending contact in our roster CM-side */
  handle = tp_handle_ensure (f->service_repo, id, NULL, NULL);
  tp_tests_contacts_connection_change_aliases (f->service_conn,
      1, &handle, &alias);
  tp_tests_contact_list_manager_request_subscription (manager, 1, &handle, message);

  /* Tell connection's factory contact features we want */
  factory = tp_proxy_get_factory (f->client_conn);
  tp_client_factory_add_contact_features_varargs (factory,
      TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_DATA,
      0);

  /* Now put it online and wait for contact list state move to success */
  g_signal_connect (f->client_conn, "contact-list-changed",
      G_CALLBACK (contact_list_changed_cb), &got_contact_list_changed);
  g_signal_connect_swapped (f->client_conn, "notify::contact-list-state",
      G_CALLBACK (finish), &result);
  tp_cli_connection_call_connect (f->client_conn, -1, NULL, NULL, NULL, NULL);
  tp_tests_proxy_run_until_prepared (f->client_conn, feature_connected);

  g_assert_cmpint (tp_connection_get_contact_list_state (f->client_conn), ==,
      TP_CONTACT_LIST_STATE_SUCCESS);

  /* SUCCESS state must have been delayed until TpContact is prepared,
     and contact-list-changed must have been emitted just before */
  g_assert (got_contact_list_changed);
  contacts = tp_connection_dup_contact_list (f->client_conn);
  g_assert (contacts != NULL);
  g_assert_cmpint (contacts->len, ==, 1);
  contact = g_ptr_array_index (contacts, 0);
  g_assert (contact != NULL);
  g_assert_cmpstr (tp_contact_get_identifier (contact), ==, id);
  g_assert_cmpstr (tp_contact_get_alias (contact), ==, alias);
  /* Even if we didn't explicitely asked that feature, we should have it for free */
  g_assert (tp_contact_has_feature (contact, TP_CONTACT_FEATURE_SUBSCRIPTION_STATES));
  g_assert_cmpint (tp_contact_get_subscribe_state (contact), ==, TP_SUBSCRIPTION_STATE_ASK);
  /* We asked for AVATAR_DATA, verify we got it. This is special because it has
   * no contact attribute, and ContactList preparation does not go through
   * the slow path. */
  g_assert (tp_contact_has_feature (contact, TP_CONTACT_FEATURE_AVATAR_DATA));

  g_ptr_array_unref (contacts);
}

static void
test_self_contact (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  const GQuark conn_features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };
  TpClientFactory *factory;
  TpContact *contact;

  factory = tp_proxy_get_factory (f->client_conn);
  tp_client_factory_add_contact_features_varargs (factory,
      TP_CONTACT_FEATURE_ALIAS,
      0);

  tp_cli_connection_call_connect (f->client_conn, -1, NULL, NULL, NULL, NULL);
  tp_tests_proxy_run_until_prepared (f->client_conn, conn_features);

  contact = tp_connection_get_self_contact (f->client_conn);
  g_assert (contact != NULL);
  g_assert (tp_contact_has_feature (contact, TP_CONTACT_FEATURE_ALIAS));
}

static void
request_subscription_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpContact *contact = (TpContact *) source;
  Result *r = user_data;

  tp_contact_request_subscription_finish (contact, result, &r->error);

  finish (r);
}

static void
test_contact_refcycle (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  const GQuark conn_features[] = { TP_CONNECTION_FEATURE_CONTACT_LIST, 0 };
  TpContact *contact1;
  TpContact *contact2;
  TpHandle handle;

  tp_tests_proxy_run_until_prepared (f->client_conn, conn_features);

  contact1 = ensure_contact (f, "contact1", &handle);
  contact2 = ensure_contact (f, "contact2", &handle);

  /* Add both contacts to roster */
  g_signal_connect_swapped (f->client_conn, "contact-list-changed",
      G_CALLBACK (finish), &f->result);

  tp_contact_request_subscription_async (contact1, "",
      request_subscription_cb, &f->result);
  tp_contact_request_subscription_async (contact2, "",
      request_subscription_cb, &f->result);

  f->result.waiting = 4;
  g_main_loop_run (f->result.loop);
  g_assert_no_error (f->result.error);

  /* At this point we own a ref to contact1, contact2 and f->client_conn.
   * The connection owns a ref to contact1 and contact2.
   * But contacts owns only a weak-ref to their connection.
   * Let's verify that's true. */
  g_object_add_weak_pointer ((GObject *) f->client_conn,
      (gpointer *) &f->client_conn);
  g_object_add_weak_pointer ((GObject *) contact1,
      (gpointer *) &contact1);
  g_object_add_weak_pointer ((GObject *) contact2,
      (gpointer *) &contact2);

  /* Connection maintains contact1 alive */
  g_object_unref (contact1);
  g_assert (contact1 != NULL);

  /* Killing the connection kills contact1 but not contact2 */
  g_object_unref (f->client_conn);
  g_assert (f->client_conn == NULL);
  g_assert (contact1 == NULL);
  g_assert (contact2 != NULL);
  g_assert (tp_contact_get_connection (contact2) == NULL);

  /* Nobody else owns a ref to contact2 now */
  g_object_unref (contact2);
  g_assert (contact2 == NULL);
}

static void
setup_internal (Fixture *f,
    gboolean connect,
    gconstpointer user_data)
{
/* TODO: we should assert that when people add new TpContact features
 * they're added to this list and tested; this was easier with
 * TpContactFeature... */
  const GQuark features[] = {
    TP_CONTACT_FEATURE_ALIAS,
    TP_CONTACT_FEATURE_AVATAR_TOKEN,
    TP_CONTACT_FEATURE_PRESENCE,
    TP_CONTACT_FEATURE_LOCATION,
    TP_CONTACT_FEATURE_CAPABILITIES,
    TP_CONTACT_FEATURE_AVATAR_DATA,
    TP_CONTACT_FEATURE_CONTACT_INFO,
    TP_CONTACT_FEATURE_CLIENT_TYPES,
    TP_CONTACT_FEATURE_SUBSCRIPTION_STATES,
    TP_CONTACT_FEATURE_CONTACT_GROUPS,
    TP_CONTACT_FEATURE_CONTACT_BLOCKING,
  };

  f->all_contact_features = g_array_new (TRUE, FALSE, sizeof (GQuark));
  g_array_append_vals (f->all_contact_features,
      features, G_N_ELEMENTS (features));

  tp_tests_create_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", connect, &f->base_connection, &f->client_conn);

  f->service_conn = TP_TESTS_CONTACTS_CONNECTION (f->base_connection);
  g_object_ref (f->service_conn);

  f->service_repo = tp_base_connection_get_handles (f->base_connection,
      TP_ENTITY_TYPE_CONTACT);
  f->result.loop = g_main_loop_new (NULL, FALSE);
}

static void
setup (Fixture *f,
    gconstpointer user_data)
{
  setup_internal (f, TRUE, user_data);
}

static void
setup_no_connect (Fixture *f,
    gconstpointer user_data)
{
  setup_internal (f, FALSE, user_data);
}

static void
teardown (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  if (f->client_conn != NULL)
    {
      TpConnection *conn = f->client_conn;

      g_object_add_weak_pointer ((GObject *) conn, (gpointer *) &conn);
      tp_tests_connection_assert_disconnect_succeeds (conn);
      g_object_unref (conn);
      g_assert (conn == NULL);
      f->client_conn = NULL;
    }

  if (f->all_contact_features != NULL)
    g_array_unref (f->all_contact_features);

  f->service_repo = NULL;
  tp_clear_object (&f->service_conn);
  tp_clear_object (&f->base_connection);

  reset_result (&f->result);
  tp_clear_pointer (&f->result.loop, g_main_loop_unref);
}

int
main (int argc,
      char **argv)
{
  gint ret;
  gchar *dir;
  GError *error = NULL;

  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  /* Make sure g_get_user_cache_dir() returns a tmp directory, to not mess up
   * user's cache dir. */
  dir = g_dir_make_tmp ("tp-glib-tests-XXXXXX", &error);
  g_assert_no_error (error);
  g_setenv ("XDG_CACHE_HOME", dir, TRUE);
  g_assert_cmpstr (g_get_user_cache_dir (), ==, dir);

#define ADD(x) \
  g_test_add ("/contacts/" #x, Fixture, NULL, setup, test_ ## x, teardown)

  ADD (no_features);
  ADD (features);
  ADD (upgrade);
  ADD (upgrade_noop);
  ADD (by_id);
  ADD (avatar_requirements);
  ADD (avatar_data);
  ADD (avatar_data_after_token);
  ADD (contact_info);
  ADD (dup_if_possible);
  ADD (subscription_states);
  ADD (contact_groups);
  ADD (no_location);

  g_test_add ("/contacts/superfluous-attributes", Fixture, NULL,
      setup_broken_client_types_conn, test_superfluous_attributes,
      teardown);

  g_test_add ("/contacts/contact-list", Fixture, NULL,
      setup_no_connect, test_contact_list, teardown);

  g_test_add ("/contacts/self-contact", Fixture, NULL,
      setup_no_connect, test_self_contact, teardown);

  ADD (contact_refcycle);

  ret = tp_tests_run_with_bus ();

  g_assert (haze_remove_directory (dir));
  g_free (dir);

  return ret;
}
