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

#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

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
    GArray *invalid;
    gchar **good_ids;
    GHashTable *bad_ids;
} Result;

typedef struct {
  Result result;
  TpBaseConnection *base_connection;
  TpBaseConnection *legacy_base_connection;
  TpBaseConnection *no_requests_base_connection;
  TpTestsContactsConnection *service_conn;
  TpHandleRepoIface *service_repo;
  TpConnection *client_conn;
  TpConnection *legacy_client_conn;
  TpConnection *no_requests_client_conn;
} Fixture;

/* We only really actively test TP_CONTACT_FEATURE_ALIAS, but preparing any
 * of these once should be enough, assuming that the CM is not broken.
 */
static TpContactFeature all_contact_features[] = {
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
  TP_CONTACT_FEATURE_CONTACT_BLOCKING
};

/* If people add new features, they should add them to this test. We could
 * generate the list dynamically but this seems less brittle.
 */
G_STATIC_ASSERT (G_N_ELEMENTS (all_contact_features) == TP_NUM_CONTACT_FEATURES);


static void
by_handle_cb (TpConnection *connection,
              guint n_contacts,
              TpContact * const *contacts,
              guint n_invalid,
              const TpHandle *invalid,
              const GError *error,
              gpointer user_data,
              GObject *weak_object)
{
  Result *result = user_data;

  g_assert (result->invalid == NULL);
  g_assert (result->contacts == NULL);
  g_assert (result->error == NULL);
  g_assert (result->good_ids == NULL);
  g_assert (result->bad_ids == NULL);

  if (error == NULL)
    {
      guint i;

      DEBUG ("got %u contacts and %u invalid", n_contacts, n_invalid);

      result->invalid = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle),
          n_invalid);
      g_array_append_vals (result->invalid, invalid, n_invalid);

      result->contacts = g_ptr_array_sized_new (n_contacts);

      for (i = 0; i < n_contacts; i++)
        {
          TpContact *contact = contacts[i];
          GFile *avatar_file;
          gchar *avatar_uri = NULL;

          avatar_file = tp_contact_get_avatar_file (contact);
          if (avatar_file != NULL)
            avatar_uri = g_file_get_uri (avatar_file);

          DEBUG ("contact #%u: %p", i, contact);
          DEBUG ("contact #%u alias: %s", i, tp_contact_get_alias (contact));
          DEBUG ("contact #%u avatar token: %s", i,
              tp_contact_get_avatar_token (contact));
          DEBUG ("contact #%u avatar MIME type: %s", i,
              tp_contact_get_avatar_mime_type (contact));
          DEBUG ("contact #%u avatar file: %s", i,
              avatar_uri);
          DEBUG ("contact #%u presence type: %u", i,
              tp_contact_get_presence_type (contact));
          DEBUG ("contact #%u presence status: %s", i,
              tp_contact_get_presence_status (contact));
          DEBUG ("contact #%u presence message: %s", i,
              tp_contact_get_presence_message (contact));
          g_ptr_array_add (result->contacts, g_object_ref (contact));

          g_free (avatar_uri);
        }
    }
  else
    {
      DEBUG ("got an error: %s %u: %s", g_quark_to_string (error->domain),
          error->code, error->message);
      result->error = g_error_copy (error);
    }
}

static void
finish (gpointer r)
{
  Result *result = r;

  g_main_loop_quit (result->loop);
}

static void
reset_result (Result *result)
{
  tp_clear_pointer (&result->invalid, g_array_unref);
  if (result->contacts != NULL)
    g_ptr_array_foreach (result->contacts, (GFunc) g_object_unref, NULL);
  tp_clear_pointer (&result->contacts, g_ptr_array_unref);
  tp_clear_pointer (&result->good_ids, g_strfreev);
  tp_clear_pointer (&result->bad_ids, g_hash_table_unref);
  g_clear_error (&result->error);
}

static void
contact_info_verify (TpContact *contact)
{
  GList *info;
  TpContactInfoField *field;

  g_assert (tp_contact_has_feature (contact, TP_CONTACT_FEATURE_CONTACT_INFO));

  info = tp_contact_get_contact_info (contact);
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

  g_list_free (info);
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

      specs = tp_connection_get_contact_info_supported_fields (connection);
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

      g_list_free (specs);
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

static void
test_contact_info (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpTestsContactsConnection *service_conn = f->service_conn;
  TpConnection *client_conn = f->client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContactFeature features[] = { TP_CONTACT_FEATURE_CONTACT_INFO };
  TpContact *contact;
  TpHandle handle;
  const gchar *field_value[] = { "Foo", NULL };
  GPtrArray *info;
  GList *info_list = NULL;
  GQuark conn_features[] = { TP_CONNECTION_FEATURE_CONTACT_INFO, 0 };
  GCancellable *cancellable;

  /* Create fake info fields */
  info = g_ptr_array_new_with_free_func ((GDestroyNotify) g_value_array_free);
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

  /* ... but first, get the SelfHandle contact without any features (regression
   * test for a related bug, fd.o #32191) */
  handle = tp_connection_get_self_handle (client_conn);
  tp_connection_get_contacts_by_handle (client_conn,
      1, &handle,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);
  g_object_ref (g_ptr_array_index (result.contacts, 0));
  reset_result (&result);

  tp_connection_set_contact_info_async (client_conn, info_list,
    contact_info_set_cb, &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  handle = tp_connection_get_self_handle (client_conn);
  tp_connection_get_contacts_by_handle (client_conn,
      1, &handle,
      G_N_ELEMENTS (features), features,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  contact = g_ptr_array_index (result.contacts, 0);
  contact_info_verify (contact);

  reset_result (&result);

  /* TEST3: Create a TpContact with the INFO feature. Then change its info in
   * the CM. That should emit "notify::info" signal on the TpContact. */
  handle = tp_handle_ensure (service_repo, "info-test-3", NULL, NULL);
  tp_connection_get_contacts_by_handle (client_conn,
      1, &handle,
      G_N_ELEMENTS (features), features,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  contact = g_ptr_array_index (result.contacts, 0);
  g_signal_connect (contact, "notify::contact-info",
      G_CALLBACK (contact_info_notify_cb), &result);

  tp_tests_contacts_connection_change_contact_info (service_conn, handle,
      info);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  reset_result (&result);
  tp_handle_unref (service_repo, handle);

  /* TEST 4: First set the info in the CM for an handle, then create a TpContact
   * without INFO feature, and finally refresh the contact's info. */
  handle = tp_handle_ensure (service_repo, "info-test-4", NULL, NULL);
  tp_tests_contacts_connection_change_contact_info (service_conn, handle,
      info);

  tp_connection_get_contacts_by_handle (client_conn,
      1, &handle,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  contact = g_ptr_array_index (result.contacts, 0);
  g_assert (tp_contact_get_contact_info (contact) == NULL);

  g_signal_connect (contact, "notify::contact-info",
      G_CALLBACK (contact_info_notify_cb), &result);
  tp_connection_refresh_contact_info (client_conn, 1, &contact);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  reset_result (&result);
  tp_handle_unref (service_repo, handle);

  /* TEST5: Create a TpContact without INFO feature, then request the contact's
   * info. */
  handle = tp_handle_ensure (service_repo, "info-test-5", NULL, NULL);
  tp_connection_get_contacts_by_handle (client_conn,
      1, &handle,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  contact = g_ptr_array_index (result.contacts, 0);
  g_assert (tp_contact_get_contact_info (contact) == NULL);

  tp_contact_request_contact_info_async (contact, NULL, contact_info_request_cb,
      &result);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  reset_result (&result);
  tp_handle_unref (service_repo, handle);

  /* TEST6: Create a TpContact without INFO feature, then request the contact's
   * info, and cancel the request. */
  handle = tp_handle_ensure (service_repo, "info-test-6", NULL, NULL);
  tp_connection_get_contacts_by_handle (client_conn,
      1, &handle,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  contact = g_ptr_array_index (result.contacts, 0);
  g_assert (tp_contact_get_contact_info (contact) == NULL);

  cancellable = g_cancellable_new ();
  tp_contact_request_contact_info_async (contact, cancellable,
      contact_info_request_cancelled_cb, &result);

  g_idle_add_full (G_PRIORITY_HIGH, contact_info_request_cancel,
      cancellable, g_object_unref);

  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  reset_result (&result);
  tp_handle_unref (service_repo, handle);

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
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  GQuark features[] = { TP_CONNECTION_FEATURE_AVATAR_REQUIREMENTS, 0 };

  g_message (G_STRFUNC);

  tp_proxy_prepare_async (TP_PROXY (client_conn), features,
      prepare_avatar_requirements_cb, &result);
  g_main_loop_run (result.loop);

  g_assert_no_error (result.error);
  g_main_loop_unref (result.loop);
}

static TpContact *
create_contact_with_fake_avatar (TpTestsContactsConnection *service_conn,
    TpConnection *client_conn,
    const gchar *id,
    gboolean request_avatar)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContactFeature feature;
  const gchar avatar_data[] = "fake-avatar-data";
  const gchar avatar_token[] = "fake-avatar-token";
  const gchar avatar_mime_type[] = "fake-avatar-mime-type";
  TpContact *contact;
  TpHandle handle;
  GArray *array;
  gchar *content = NULL;

  handle = tp_handle_ensure (service_repo, id, NULL, NULL);
  array = g_array_new (FALSE, FALSE, sizeof (gchar));
  g_array_append_vals (array, avatar_data, strlen (avatar_data) + 1);

  tp_tests_contacts_connection_change_avatar_data (service_conn, handle, array,
      avatar_mime_type, avatar_token);

  if (request_avatar)
    feature = TP_CONTACT_FEATURE_AVATAR_DATA;
  else
    feature = TP_CONTACT_FEATURE_AVATAR_TOKEN;

  tp_connection_get_contacts_by_handle (client_conn,
      1, &handle,
      1, &feature,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  contact = g_object_ref (g_ptr_array_index (result.contacts, 0));

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

  tp_handle_unref (service_repo, handle);
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
  TpTestsContactsConnection *service_conn = f->service_conn;
  TpConnection *client_conn = f->client_conn;
  gboolean avatar_retrieved_called;
  GError *error = NULL;
  TpContact *contact1, *contact2;
  TpProxySignalConnection *signal_id;

  g_message (G_STRFUNC);

  /* Check if AvatarRetrieved gets called */
  signal_id = tp_cli_connection_interface_avatars_connect_to_avatar_retrieved (
      client_conn, avatar_retrieved_cb, &avatar_retrieved_called, NULL, NULL,
      &error);
  g_assert_no_error (error);

  /* First time we create a contact, avatar should not be in cache, so
   * AvatarRetrived should be called */
  avatar_retrieved_called = FALSE;
  contact1 = create_contact_with_fake_avatar (service_conn, client_conn,
      "fake-id1", TRUE);
  g_assert (avatar_retrieved_called);
  g_assert (contact1 != NULL);
  g_assert (tp_contact_get_avatar_file (contact1) != NULL);

  /* Second time we create a contact, avatar should be in cache now, so
   * AvatarRetrived should NOT be called */
  avatar_retrieved_called = FALSE;
  contact2 = create_contact_with_fake_avatar (service_conn, client_conn,
      "fake-id2", TRUE);
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
  TpTestsContactsConnection *service_conn = f->service_conn;
  TpConnection *client_conn = f->client_conn;
  const gchar *id = "avatar-data-after-token";
  TpContact *contact1, *contact2;

  g_message (G_STRFUNC);

  /* Create a contact with AVATAR_TOKEN feature */
  contact1 = create_contact_with_fake_avatar (service_conn, client_conn, id,
      FALSE);
  g_assert (contact1 != NULL);
  g_assert (tp_contact_get_avatar_file (contact1) == NULL);

  /* Now create the same contact with AVATAR_DATA feature */
  contact2 = create_contact_with_fake_avatar (service_conn, client_conn, id,
      TRUE);
  g_assert (contact2 != NULL);
  g_assert (tp_contact_get_avatar_file (contact2) != NULL);

  g_assert (contact1 == contact2);

  /* Cleanup */
  g_object_unref (contact1);
  g_object_unref (contact2);
}

static void
test_by_handle (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpTestsContactsConnection *service_conn = f->service_conn;
  TpConnection *client_conn = f->client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handles[5] = { 0, 0, 0, 0, 0 };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[4];
  gpointer weak_pointers[4];
  guint i;

  g_message (G_STRFUNC);

  /* arrange for some handles to exist */
  handles[0] = tp_handle_ensure (service_repo, "alice", NULL, NULL);
  MYASSERT (handles[0] != 0, "");
  handles[1] = tp_handle_ensure (service_repo, "bob", NULL, NULL);
  MYASSERT (handles[1] != 0, "");
  /* randomly guess at a handle that shouldn't exist */
  handles[2] = 31337;
  MYASSERT (!tp_handle_is_valid (service_repo, 31337, NULL), "");
  /* another valid handle */
  handles[3] = tp_handle_ensure (service_repo, "chris", NULL, NULL);
  MYASSERT (handles[3] != 0, "");
  /* another invalid handle */
  handles[4] = 12345;
  MYASSERT (!tp_handle_is_valid (service_repo, 12345, NULL), "");

  /* Make a request for the following 5 contacts:
   * - alice
   * - bob
   * - invalid handle 31337
   * - chris
   * - invalid handle 12345
   */
  tp_connection_get_contacts_by_handle (client_conn,
      5, handles,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 2, ": %u", result.invalid->len);
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");
  contacts[0] = g_object_ref (g_ptr_array_index (result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (contacts[0]), ==, handles[0]);
  g_assert_cmpstr (tp_contact_get_identifier (contacts[0]), ==, "alice");
  contacts[1] = g_object_ref (g_ptr_array_index (result.contacts, 1));
  g_assert_cmpuint (tp_contact_get_handle (contacts[1]), ==, handles[1]);
  g_assert_cmpstr (tp_contact_get_identifier (contacts[1]), ==, "bob");
  contacts[3] = g_object_ref (g_ptr_array_index (result.contacts, 2));
  g_assert_cmpuint (tp_contact_get_handle (contacts[3]), ==, handles[3]);
  g_assert_cmpstr (tp_contact_get_identifier (contacts[3]), ==, "chris");

  /* clean up before doing the second request */
  reset_result (&result);
  g_assert (result.error == NULL);

  /* Replace one of the invalid handles with a valid one */
  handles[2] = tp_handle_ensure (service_repo, "dora", NULL, NULL);
  MYASSERT (handles[2] != 0, "");

  /* Make a request for the following 4 contacts:
   * - alice (TpContact exists)
   * - bob (TpContact exists)
   * - dora (TpContact needs to be created)
   * - chris (TpContact exists)
   */
  tp_connection_get_contacts_by_handle (client_conn,
      4, handles,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  /* assert that we got the same contacts back */

  MYASSERT (result.contacts->len == 4, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  g_assert_no_error (result.error);

  /* 0, 1 and 3 we already have a reference to */
  MYASSERT (g_ptr_array_index (result.contacts, 0) == contacts[0], "");
  g_object_unref (g_ptr_array_index (result.contacts, 0));
  MYASSERT (g_ptr_array_index (result.contacts, 1) == contacts[1], "");
  g_object_unref (g_ptr_array_index (result.contacts, 1));
  MYASSERT (g_ptr_array_index (result.contacts, 3) == contacts[3], "");
  g_object_unref (g_ptr_array_index (result.contacts, 3));

  /* 2 we don't */
  contacts[2] = g_ptr_array_index (result.contacts, 2);
  g_assert_cmpuint (tp_contact_get_handle (contacts[2]), ==, handles[2]);
  g_assert_cmpstr (tp_contact_get_identifier (contacts[2]), ==, "dora");

  g_ptr_array_unref (result.contacts);
  result.contacts = NULL;

  /* clean up refs to contacts and assert that they aren't leaked */

  for (i = 0; i < 4; i++)
    {
      weak_pointers[i] = contacts[i];
      g_object_add_weak_pointer ((GObject *) contacts[i],weak_pointers +i);
    }

  for (i = 0; i < 4; i++)
    {
      g_object_unref (contacts[i]);
      MYASSERT (weak_pointers[i] == NULL, ": %u", i);
    }

  /* wait for ReleaseHandles to run */
  tp_tests_proxy_run_until_dbus_queue_processed (client_conn);

  /* remaining cleanup */
  g_assert (result.error == NULL);
  reset_result (&result);
  g_main_loop_unref (result.loop);
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

  tp_dbus_daemon_unregister_object (
      tp_base_connection_get_dbus_daemon (f->base_connection),
      f->base_connection);
  /* check that that worked */
  ok = tp_cli_connection_run_get_self_handle (f->client_conn, -1, NULL,
      &error, NULL);
  g_assert_error (error, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD);
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

  tp_dbus_daemon_register_object (
      tp_base_connection_get_dbus_daemon (f->base_connection),
      tp_base_connection_get_object_path (f->base_connection),
      f->base_connection);
  /* check that *that* worked */
  ok = tp_cli_connection_run_get_self_handle (f->client_conn, -1, NULL,
      &error, NULL);
  g_assert_no_error (error);
  g_assert (ok);
}

static TpHandle
get_handle_with_no_caps (Fixture *f,
    const gchar *id)
{
  TpHandle handle;
  GHashTable *capabilities;

  handle = tp_handle_ensure (f->service_repo, id, NULL, NULL);
  g_assert_cmpuint (handle, !=, 0);

  /* Unlike almost every other feature, with capabilities “not sure” and “none”
   * are different: you really might care about the difference between “I don't
   * know if blah can do video” versus “I know blah cannot do video”.
   *
   * It happens that we get the repeated-reintrospection behaviour for the
   * former case of contact caps. I can't really be bothered to fix this.
   */
  capabilities = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_ptr_array_unref);
  g_hash_table_insert (capabilities, GUINT_TO_POINTER (handle),
      g_ptr_array_new ());
  tp_tests_contacts_connection_change_capabilities (f->service_conn,
      capabilities);
  g_hash_table_unref (capabilities);

  return handle;
}

static void
test_by_handle_again (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handle;
  TpContact *contact;
  gpointer weak_pointer;
  const gchar *alias = "Alice in Wonderland";

  g_test_bug ("25181");

  handle = get_handle_with_no_caps (f, "alice");
  tp_tests_contacts_connection_change_aliases (f->service_conn, 1, &handle,
      &alias);

  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &handle,
      G_N_ELEMENTS (all_contact_features), all_contact_features,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_cmpuint (result.contacts->len, ==, 1);
  g_assert_cmpuint (result.invalid->len, ==, 0);
  g_assert_no_error (result.error);

  g_assert (g_ptr_array_index (result.contacts, 0) != NULL);
  contact = g_object_ref (g_ptr_array_index (result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (contact), ==, handle);
  g_assert_cmpstr (tp_contact_get_identifier (contact), ==, "alice");
  g_assert_cmpstr (tp_contact_get_alias (contact), ==, "Alice in Wonderland");

  /* clean up before doing the second request */
  reset_result (&result);
  g_assert (result.error == NULL);

  make_the_connection_disappear (f);

  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &handle,
      G_N_ELEMENTS (all_contact_features), all_contact_features,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);
  g_assert_cmpuint (result.contacts->len, ==, 1);
  g_assert_cmpuint (result.invalid->len, ==, 0);

  g_assert (g_ptr_array_index (result.contacts, 0) == contact);
  g_assert_cmpstr (tp_contact_get_alias (contact), ==, "Alice in Wonderland");

  put_the_connection_back (f);

  g_assert (result.error == NULL);
  reset_result (&result);

  weak_pointer = contact;
  g_object_add_weak_pointer ((GObject *) contact, &weak_pointer);
  g_object_unref (contact);
  g_assert (weak_pointer == NULL);

  tp_tests_proxy_run_until_dbus_queue_processed (f->client_conn);
  g_main_loop_unref (result.loop);
}

static void
test_by_handle_upgrade (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handle;
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      f->base_connection, TP_HANDLE_TYPE_CONTACT);
  TpContact *contact;
  gpointer weak_pointer;
  const gchar *alias = "Alice in Wonderland";
  TpContactFeature feature = TP_CONTACT_FEATURE_ALIAS;

  g_test_bug ("32191");

  handle = tp_handle_ensure (service_repo, "alice", NULL, NULL);
  g_assert_cmpuint (handle, !=, 0);
  tp_tests_contacts_connection_change_aliases (f->service_conn, 1, &handle,
      &alias);

  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &handle,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_cmpuint (result.contacts->len, ==, 1);
  g_assert_cmpuint (result.invalid->len, ==, 0);
  g_assert_no_error (result.error);

  g_assert (g_ptr_array_index (result.contacts, 0) != NULL);
  contact = g_object_ref (g_ptr_array_index (result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (contact), ==, handle);
  g_assert_cmpstr (tp_contact_get_identifier (contact), ==, "alice");
  /* fallback alias is still in effect */
  g_assert_cmpstr (tp_contact_get_alias (contact), ==, "alice");

  /* clean up before doing the second request */
  reset_result (&result);
  g_assert (result.error == NULL);

  /* the second request enables the Alias feature, so it must make more D-Bus
   * round trips */
  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &handle,
      1, &feature,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_cmpuint (result.contacts->len, ==, 1);
  g_assert_cmpuint (result.invalid->len, ==, 0);
  g_assert_no_error (result.error);

  g_assert (g_ptr_array_index (result.contacts, 0) == contact);
  g_assert_cmpstr (tp_contact_get_alias (contact), ==, "Alice in Wonderland");

  g_assert (result.error == NULL);
  reset_result (&result);

  weak_pointer = contact;
  g_object_add_weak_pointer ((GObject *) contact, &weak_pointer);
  g_object_unref (contact);
  g_assert (weak_pointer == NULL);

  tp_tests_proxy_run_until_dbus_queue_processed (f->client_conn);
  g_main_loop_unref (result.loop);
}

static void
test_no_features (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpTestsContactsConnection *service_conn = f->service_conn;
  TpConnection *client_conn = f->client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  const gchar * const ids[] = { "alice", "bob", "chris" };
  TpHandle handles[3] = { 0, 0, 0 };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[3];
  guint i;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    handles[i] = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

  tp_connection_get_contacts_by_handle (client_conn,
      3, handles,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_object_ref (g_ptr_array_index (result.contacts, i));

  g_assert (result.error == NULL);
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
      tp_handle_unref (service_repo, handles[i]);
    }

  /* remaining cleanup */
  g_main_loop_unref (result.loop);
}

static void
upgrade_cb (TpConnection *connection,
            guint n_contacts,
            TpContact * const *contacts,
            const GError *error,
            gpointer user_data,
            GObject *weak_object)
{
  Result *result = user_data;

  g_assert (result->invalid == NULL);
  g_assert (result->contacts == NULL);
  g_assert (result->error == NULL);
  g_assert (result->good_ids == NULL);
  g_assert (result->bad_ids == NULL);

  if (error == NULL)
    {
      guint i;

      DEBUG ("got %u contacts", n_contacts);

      result->contacts = g_ptr_array_sized_new (n_contacts);

      for (i = 0; i < n_contacts; i++)
        {
          TpContact *contact = contacts[i];

          DEBUG ("contact #%u: %p", i, contact);
          DEBUG ("contact #%u alias: %s", i, tp_contact_get_alias (contact));
          DEBUG ("contact #%u avatar token: %s", i,
              tp_contact_get_avatar_token (contact));
          DEBUG ("contact #%u presence type: %u", i,
              tp_contact_get_presence_type (contact));
          DEBUG ("contact #%u presence status: %s", i,
              tp_contact_get_presence_status (contact));
          DEBUG ("contact #%u presence message: %s", i,
              tp_contact_get_presence_message (contact));
          g_ptr_array_add (result->contacts, g_object_ref (contact));
        }
    }
  else
    {
      DEBUG ("got an error: %s %u: %s", g_quark_to_string (error->domain),
          error->code, error->message);
      result->error = g_error_copy (error);
    }
}

/* Just put a country in locations for easier comparaisons.
 * FIXME: Ideally we should have a MYASSERT_SAME_ASV */
#define ASSERT_SAME_LOCATION(left, left_vardict, right)\
  G_STMT_START {\
    g_assert_cmpuint (g_hash_table_size (left), ==, \
        g_hash_table_size (right));\
    g_assert_cmpstr (tp_asv_get_string (left, "country"), ==,\
        tp_asv_get_string (right, "country"));\
    \
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
    TpHandleType handle_type)
{
  GHashTable *fixed;
  const gchar * const allowed[] = { NULL };
  GValueArray *arr;

  fixed = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          handle_type,
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
  add_text_chat_class (caps1, TP_HANDLE_TYPE_CONTACT);
  g_hash_table_insert (capabilities, GUINT_TO_POINTER (handles[0]), caps1);

  /* Support text chatrooms */
  caps2 = g_ptr_array_sized_new (1);
  add_text_chat_class (caps2, TP_HANDLE_TYPE_ROOM);
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
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
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
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[3];
  TpContactFeature features[] = { TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN, TP_CONTACT_FEATURE_PRESENCE,
      TP_CONTACT_FEATURE_LOCATION, TP_CONTACT_FEATURE_CAPABILITIES };
  guint i;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    handles[i] = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

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

  tp_connection_get_contacts_by_handle (client_conn,
      3, handles,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_object_ref (g_ptr_array_index (result.contacts, i));

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

  tp_connection_upgrade_contacts (client_conn,
      3, contacts,
      G_N_ELEMENTS (features), features,
      upgrade_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid == NULL, "");
  g_assert_no_error (result.error);

  for (i = 0; i < 3; i++)
    {
      MYASSERT (g_ptr_array_index (result.contacts, 0) == contacts[0], "");
    }

  g_assert (result.invalid == NULL);
  g_assert (result.error == NULL);
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
      ASSERT_SAME_LOCATION (tp_contact_get_location (contacts[i]),
          vardict, locations[i]);
      g_variant_unref (vardict);

      g_object_get (contacts[i],
          "location-vardict", &vardict,
          NULL);
      ASSERT_SAME_LOCATION (tp_contact_get_location (contacts[i]),
          vardict, locations[i]);
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
      tp_handle_unref (service_repo, handles[i]);
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
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handle;
  TpContact *contact;

  g_test_bug ("41414");

  /* Get a contact by handle */
  handle = get_handle_with_no_caps (f, "test-upgrade-noop");
  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &handle,
      G_N_ELEMENTS (all_contact_features), all_contact_features,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

  contact = g_object_ref (g_ptr_array_index (result.contacts, 0));
  reset_result (&result);

  /* Upgrade it, but it should already have all features */
  make_the_connection_disappear (f);
  tp_connection_upgrade_contacts (f->client_conn,
      1, &contact,
      G_N_ELEMENTS (all_contact_features), all_contact_features,
      upgrade_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);
  reset_result (&f->result);

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
  gboolean location_vardict_changed;
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
  ctx->location_vardict_changed = FALSE;
  ctx->capabilities_changed = FALSE;
}

static gboolean
notify_ctx_is_fully_changed (notify_ctx *ctx)
{
  return ctx->alias_changed && ctx->avatar_token_changed &&
    ctx->presence_type_changed && ctx->presence_status_changed &&
    ctx->presence_msg_changed && ctx->location_changed &&
    ctx->location_vardict_changed && ctx->capabilities_changed;
}

static gboolean
notify_ctx_is_changed (notify_ctx *ctx)
{
  return ctx->alias_changed || ctx->avatar_token_changed ||
    ctx->presence_type_changed || ctx->presence_status_changed ||
    ctx->presence_msg_changed || ctx->location_changed ||
    ctx->location_vardict_changed || ctx->capabilities_changed;
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
  else if (!tp_strdiff (param->name, "location-vardict"))
    ctx->location_vardict_changed = TRUE;
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
  add_text_chat_class (caps1, TP_HANDLE_TYPE_CONTACT);
  add_text_chat_class (caps1, TP_HANDLE_TYPE_ROOM);
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
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
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
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[3];
  TpContactFeature features[] = { TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN, TP_CONTACT_FEATURE_PRESENCE,
      TP_CONTACT_FEATURE_LOCATION, TP_CONTACT_FEATURE_CAPABILITIES };
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
      GHashTable *location;
      GVariant *location_vardict;
      TpCapabilities *capabilities;
  } from_gobject;
  notify_ctx notify_ctx_alice, notify_ctx_chris;
  GVariant *vardict;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    handles[i] = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

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

  tp_connection_get_contacts_by_handle (client_conn,
      3, handles,
      G_N_ELEMENTS (features), features,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_object_ref (g_ptr_array_index (result.contacts, i));

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
      ASSERT_SAME_LOCATION (tp_contact_get_location (contacts[i]),
          vardict, locations[i]);
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
      "location", &from_gobject.location,
      "location-vardict", &from_gobject.location_vardict,
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
  ASSERT_SAME_LOCATION (from_gobject.location, from_gobject.location_vardict,
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
  g_hash_table_unref (from_gobject.location);
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
      ASSERT_SAME_LOCATION (tp_contact_get_location (contacts[i]),
          vardict, new_locations[i]);
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
      tp_handle_unref (service_repo, handles[i]);
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
by_id_cb (TpConnection *connection,
          guint n_contacts,
          TpContact * const *contacts,
          const gchar * const *good_ids,
          GHashTable *bad_ids,
          const GError *error,
          gpointer user_data,
          GObject *weak_object)
{
  Result *result = user_data;

  g_assert (result->invalid == NULL);
  g_assert (result->contacts == NULL);
  g_assert (result->error == NULL);
  g_assert (result->good_ids == NULL);
  g_assert (result->bad_ids == NULL);

  if (error == NULL)
    {
      GHashTableIter iter;
      gpointer key, value;
      guint i;

      DEBUG ("got %u contacts and %u bad IDs", n_contacts,
          g_hash_table_size (bad_ids));

      result->bad_ids = g_hash_table_new_full (g_str_hash, g_str_equal,
          g_free, (GDestroyNotify) g_error_free);
      tp_g_hash_table_update (result->bad_ids, bad_ids,
          (GBoxedCopyFunc) g_strdup, (GBoxedCopyFunc) g_error_copy);

      g_hash_table_iter_init (&iter, result->bad_ids);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          gchar *id = key;
          GError *e = value;

          DEBUG ("bad ID %s: %s %u: %s", id, g_quark_to_string (e->domain),
              e->code, e->message);
        }

      result->good_ids = g_strdupv ((GStrv) good_ids);

      result->contacts = g_ptr_array_sized_new (n_contacts);

      for (i = 0; i < n_contacts; i++)
        {
          TpContact *contact = contacts[i];

          DEBUG ("contact #%u: %p", i, contact);
          DEBUG ("contact #%u we asked for ID %s", i, good_ids[i]);
          DEBUG ("contact #%u we got ID %s", i,
              tp_contact_get_identifier (contact));
          DEBUG ("contact #%u alias: %s", i, tp_contact_get_alias (contact));
          DEBUG ("contact #%u avatar token: %s", i,
              tp_contact_get_avatar_token (contact));
          DEBUG ("contact #%u presence type: %u", i,
              tp_contact_get_presence_type (contact));
          DEBUG ("contact #%u presence status: %s", i,
              tp_contact_get_presence_status (contact));
          DEBUG ("contact #%u presence message: %s", i,
              tp_contact_get_presence_message (contact));
          g_ptr_array_add (result->contacts, g_object_ref (contact));
        }
    }
  else
    {
      DEBUG ("got an error: %s %u: %s", g_quark_to_string (error->domain),
          error->code, error->message);
      result->error = g_error_copy (error);
    }
}


static void
test_by_id (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpConnection *client_conn = f->client_conn;
  Result result = { g_main_loop_new (NULL, FALSE) };
  static const gchar * const ids[] = { "Alice", "Bob", "Not valid", "Chris",
      "not valid either", NULL };
  TpContact *contacts[3];
  GError *e /* no initialization needed */;

  g_message ("%s: all bad (fd.o #19688)", G_STRFUNC);

  tp_connection_get_contacts_by_id (client_conn,
      1, ids + 2,
      0, NULL,
      by_id_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 0, ": %u", result.contacts->len);
  MYASSERT (g_hash_table_size (result.bad_ids) == 1, ": %u",
      g_hash_table_size (result.bad_ids));
  g_assert_no_error (result.error);

  e = g_hash_table_lookup (result.bad_ids, "Not valid");
  MYASSERT (e != NULL, "");

  reset_result (&result);

  g_message ("%s: all good", G_STRFUNC);

  tp_connection_get_contacts_by_id (client_conn,
      2, ids,
      0, NULL,
      by_id_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 2, ": %u", result.contacts->len);
  MYASSERT (g_hash_table_size (result.bad_ids) == 0, ": %u",
      g_hash_table_size (result.bad_ids));
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  contacts[0] = g_ptr_array_index (result.contacts, 0);
  g_assert_cmpstr (result.good_ids[0], ==, "Alice");
  g_assert_cmpstr (tp_contact_get_identifier (contacts[0]), ==, "alice");
  contacts[1] = g_ptr_array_index (result.contacts, 1);
  g_assert_cmpstr (result.good_ids[1], ==, "Bob");
  g_assert_cmpstr (tp_contact_get_identifier (contacts[1]), ==, "bob");

  reset_result (&result);

  g_message ("%s: not all good", G_STRFUNC);

  tp_connection_get_contacts_by_id (client_conn,
      5, ids,
      0, NULL,
      by_id_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (g_hash_table_size (result.bad_ids) == 2, ": %u",
      g_hash_table_size (result.bad_ids));
  g_assert_no_error (result.error);

  e = g_hash_table_lookup (result.bad_ids, "Not valid");
  MYASSERT (e != NULL, "");

  e = g_hash_table_lookup (result.bad_ids, "not valid either");
  MYASSERT (e != NULL, "");

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");
  contacts[0] = g_ptr_array_index (result.contacts, 0);
  g_assert_cmpstr (result.good_ids[0], ==, "Alice");
  g_assert_cmpstr (tp_contact_get_identifier (contacts[0]), ==, "alice");
  contacts[1] = g_ptr_array_index (result.contacts, 1);
  g_assert_cmpstr (result.good_ids[1], ==, "Bob");
  g_assert_cmpstr (tp_contact_get_identifier (contacts[1]), ==, "bob");
  contacts[2] = g_ptr_array_index (result.contacts, 2);
  g_assert_cmpstr (result.good_ids[2], ==, "Chris");
  g_assert_cmpstr (tp_contact_get_identifier (contacts[2]), ==, "chris");

  /* wait for ReleaseHandles to run */
  tp_tests_proxy_run_until_dbus_queue_processed (client_conn);

  /* remaining cleanup */
  reset_result (&result);
  g_main_loop_unref (result.loop);
}


static void
test_capabilities_without_contact_caps (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpConnection *client_conn = f->legacy_client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handles[] = { 0, 0, 0 };
  static const gchar * const ids[] = { "alice", "bob", "chris" };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      f->legacy_base_connection, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[3];
  guint i;
  TpContactFeature features[] = { TP_CONTACT_FEATURE_CAPABILITIES };

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    handles[i] = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

  tp_connection_get_contacts_by_handle (client_conn,
      3, handles,
      G_N_ELEMENTS (features), features,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_ptr_array_index (result.contacts, i);

  for (i = 0; i < 3; i++)
    {
      TpCapabilities *caps;

      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==,
          ids[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_CAPABILITIES), "");

      caps = tp_contact_get_capabilities (contacts[i]);
      MYASSERT (caps != NULL, "");
      MYASSERT (!tp_capabilities_is_specific_to_contact (caps), "");
      MYASSERT (!tp_capabilities_supports_text_chats (caps), " contact %u", i);
      MYASSERT (!tp_capabilities_supports_text_chatrooms (caps),
          " contact %u", i);
    }

  g_assert (result.error == NULL);
  reset_result (&result);
  g_main_loop_unref (result.loop);
}


static void
test_prepare_contact_caps_without_request (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpConnection *client_conn = f->no_requests_client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handles[] = { 0, 0, 0 };
  static const gchar * const ids[] = { "alice", "bob", "chris" };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      f->no_requests_base_connection, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[3];
  guint i;
  TpContactFeature features[] = { TP_CONTACT_FEATURE_CAPABILITIES };

  g_test_bug ("27686");

  for (i = 0; i < 3; i++)
    handles[i] = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

  tp_connection_get_contacts_by_handle (client_conn,
      3, handles,
      G_N_ELEMENTS (features), features,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_ptr_array_index (result.contacts, i);

  for (i = 0; i < 3; i++)
    {
      TpCapabilities *caps;

      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==,
          ids[i]);

      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_CAPABILITIES), "");

      caps = tp_contact_get_capabilities (contacts[i]);
      MYASSERT (caps == NULL, "");
    }

  g_assert (result.error == NULL);
  reset_result (&result);
  g_main_loop_unref (result.loop);
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

  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &alice_handle,
      0, NULL,
      by_handle_cb,
      &f->result, finish, NULL);
  g_main_loop_run (f->result.loop);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);
  g_assert_cmpuint (f->result.invalid->len, ==, 0);
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
}

typedef struct
{
  TpSubscriptionState subscribe;
  TpSubscriptionState publish;
  const gchar *publish_request;

  GMainLoop *loop;
} SubscriptionStates;

static void
assert_subscription_states (TpContact *contact,
    SubscriptionStates *states)
{
  g_assert_cmpint (tp_contact_get_subscribe_state (contact), ==, states->subscribe);
  g_assert_cmpint (tp_contact_get_publish_state (contact), ==, states->publish);
  g_assert_cmpstr (tp_contact_get_publish_request (contact), ==, states->publish_request);
}

static void
subscription_states_changed_cb (TpContact *contact,
    TpSubscriptionState subscribe,
    TpSubscriptionState publish,
    const gchar *publish_request,
    SubscriptionStates *states)
{
  assert_subscription_states (contact, states);
  g_main_loop_quit (states->loop);
}

static void
test_subscription_states (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle alice_handle;
  TpContact *alice;
  TpTestsContactListManager *manager;
  TpContactFeature features[] = { TP_CONTACT_FEATURE_SUBSCRIPTION_STATES };
  SubscriptionStates states = { TP_SUBSCRIPTION_STATE_NO,
      TP_SUBSCRIPTION_STATE_NO, "", f->result.loop };

  manager = tp_tests_contacts_connection_get_contact_list_manager (
      f->service_conn);

  alice_handle = tp_handle_ensure (f->service_repo, "alice", NULL, NULL);
  g_assert_cmpuint (alice_handle, !=, 0);

  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &alice_handle,
      G_N_ELEMENTS (features), features,
      by_handle_cb,
      &f->result, finish, NULL);
  g_main_loop_run (f->result.loop);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);
  g_assert_cmpuint (f->result.invalid->len, ==, 0);
  g_assert_no_error (f->result.error);

  g_assert (g_ptr_array_index (f->result.contacts, 0) != NULL);
  alice = g_object_ref (g_ptr_array_index (f->result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (alice), ==, alice_handle);
  g_assert_cmpstr (tp_contact_get_identifier (alice), ==, "alice");
  assert_subscription_states (alice, &states);

  reset_result (&f->result);

  g_signal_connect (alice, "subscription-states-changed",
      G_CALLBACK (subscription_states_changed_cb), &states);

  /* Request subscription */
  tp_tests_contact_list_manager_request_subscription (manager, 1, &alice_handle, "");
  states.subscribe = TP_SUBSCRIPTION_STATE_ASK;
  g_main_loop_run (states.loop);

  /* Request again must re-emit the signal. Saying please this time will make
   * the request accepted and will ask for publish. */
  tp_tests_contact_list_manager_request_subscription (manager, 1, &alice_handle, "please");
  g_main_loop_run (states.loop);
  states.subscribe = TP_SUBSCRIPTION_STATE_YES;
  states.publish = TP_SUBSCRIPTION_STATE_ASK;
  states.publish_request = "automatic publish request";
  g_main_loop_run (states.loop);

  /* Remove the contact */
  tp_tests_contact_list_manager_remove (manager, 1, &alice_handle);
  states.subscribe = TP_SUBSCRIPTION_STATE_NO;
  states.publish = TP_SUBSCRIPTION_STATE_NO;
  states.publish_request = "";
  g_main_loop_run (states.loop);

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
  TpContactFeature features[] = { TP_CONTACT_FEATURE_CONTACT_GROUPS };
  ContactGroups data;

  data.groups = g_ptr_array_new ();
  data.loop = f->result.loop;

  manager = tp_tests_contacts_connection_get_contact_list_manager (
      f->service_conn);

  alice_handle = tp_handle_ensure (f->service_repo, "alice", NULL, NULL);
  g_assert_cmpuint (alice_handle, !=, 0);

  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &alice_handle,
      G_N_ELEMENTS (features), features,
      by_handle_cb,
      &f->result, finish, NULL);
  g_main_loop_run (f->result.loop);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);
  g_assert_cmpuint (f->result.invalid->len, ==, 0);
  g_assert_no_error (f->result.error);

  g_assert (g_ptr_array_index (f->result.contacts, 0) != NULL);
  alice = g_object_ref (g_ptr_array_index (f->result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (alice), ==, alice_handle);
  g_assert_cmpstr (tp_contact_get_identifier (alice), ==, "alice");
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
  GHashTable *retrieved_location = tp_contact_get_location (contact);

  if (retrieved_location != NULL)
    g_assert (g_hash_table_size (retrieved_location) == 0);
}

/* This is a regression test for an issue where the LOCATION feature would
 * never be marked as prepared for contacts with no published location, so
 * repeated calls to tp_connection_get_contacts_by_handle() would call
 * GetContactAttributes() over and over. It's really a special case of
 * test_by_handle_again(), but presented separately for clarity.
 */
static void
test_no_location (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle handle;
  TpContact *contact;
  gpointer weak_pointer;
  TpContactFeature feature = TP_CONTACT_FEATURE_LOCATION;
  GHashTable *norway = tp_asv_new ("country",  G_TYPE_STRING, "Norway", NULL);
  notify_ctx notify_ctx_alice;
  GVariant *vardict;

  g_test_bug ("39377");

  handle = tp_handle_ensure (f->service_repo, "alice", NULL, NULL);
  g_assert_cmpuint (handle, !=, 0);

  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &handle,
      1, &feature,
      by_handle_cb,
      &f->result, finish, NULL);
  g_main_loop_run (f->result.loop);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);
  g_assert_cmpuint (f->result.invalid->len, ==, 0);
  g_assert_no_error (f->result.error);

  g_assert (g_ptr_array_index (f->result.contacts, 0) != NULL);
  contact = g_object_ref (g_ptr_array_index (f->result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (contact), ==, handle);
  assert_no_location (contact);
  reset_result (&f->result);

  /* Although Alice doesn't have a published location, the feature's still been
   * prepared, so we shouldn't need any D-Bus traffic to re-fetch her TpContact.
   */
  make_the_connection_disappear (f);
  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &handle,
      1, &feature,
      by_handle_cb,
      &f->result, finish, NULL);
  g_main_loop_run (f->result.loop);
  g_assert_no_error (f->result.error);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);
  g_assert_cmpuint (f->result.invalid->len, ==, 0);

  g_assert (g_ptr_array_index (f->result.contacts, 0) == contact);
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
  g_assert (notify_ctx_alice.location_vardict_changed);
  vardict = tp_contact_dup_location (contact);
  ASSERT_SAME_LOCATION (tp_contact_get_location (contact), vardict, norway);
  g_variant_unref (vardict);

  weak_pointer = contact;
  g_object_add_weak_pointer ((GObject *) contact, &weak_pointer);
  g_object_unref (contact);
  g_assert (weak_pointer == NULL);

  /* Check that first retrieving a contact without the LOCATION feature, and
   * later upgrading it to have the LOCATION feature, does the right thing.
   */
  handle = tp_handle_ensure (f->service_repo, "rupert", NULL, NULL);
  g_assert_cmpuint (handle, !=, 0);

  tp_tests_contacts_connection_change_locations (f->service_conn,
      1, &handle, &norway);

  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &handle,
      0, NULL,
      by_handle_cb,
      &f->result, finish, NULL);
  g_main_loop_run (f->result.loop);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);
  g_assert_cmpuint (f->result.invalid->len, ==, 0);
  g_assert_no_error (f->result.error);

  g_assert (g_ptr_array_index (f->result.contacts, 0) != NULL);
  contact = g_object_ref (g_ptr_array_index (f->result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (contact), ==, handle);
  assert_no_location (contact);

  /* clean up before doing the second request */
  reset_result (&f->result);

  tp_connection_upgrade_contacts (f->client_conn,
      1, &contact,
      1, &feature,
      upgrade_cb,
      &f->result, finish, NULL);
  g_main_loop_run (f->result.loop);
  g_assert_no_error (f->result.error);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);

  g_assert (g_ptr_array_index (f->result.contacts, 0) == contact);
  vardict = tp_contact_dup_location (contact);
  ASSERT_SAME_LOCATION (tp_contact_get_location (contact), vardict, norway);
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
      TP_HANDLE_TYPE_CONTACT);
  f->result.loop = g_main_loop_new (NULL, FALSE);
}

static void
test_superfluous_attributes (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle handle;
  TpContact *contact;
  const gchar * const *client_types;
  TpContactFeature client_types_feature = TP_CONTACT_FEATURE_CLIENT_TYPES;
  TpContactFeature presence_feature = TP_CONTACT_FEATURE_PRESENCE;

  g_assert (TP_TESTS_IS_BROKEN_CLIENT_TYPES_CONNECTION (f->service_conn));

  handle = tp_handle_ensure (f->service_repo, "helge", NULL, NULL);
  g_assert_cmpuint (handle, !=, 0);

  /* We ask for ClientTypes; the CM is broken and adds SimplePresence
   * information to the reply... it also omits the /client-types attribute from
   * the reply, which, since the spec says “Omitted from the result if the
   * contact's client types are not known.” leaves us in the exciting position
   * of having to decide between marking the feature as prepared anyway or
   * saying it failed, and also deciding whether get_client_types returns [] or
   * NULL...
   */
  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &handle,
      1, &client_types_feature,
      by_handle_cb,
      &f->result, finish, NULL);
  g_main_loop_run (f->result.loop);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);
  g_assert_cmpuint (f->result.invalid->len, ==, 0);
  g_assert_no_error (f->result.error);

  g_assert (g_ptr_array_index (f->result.contacts, 0) != NULL);
  contact = g_object_ref (g_ptr_array_index (f->result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (contact), ==, handle);

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
  tp_connection_get_contacts_by_handle (f->client_conn,
      1, &handle,
      1, &presence_feature,
      by_handle_cb,
      &f->result, finish, NULL);
  g_main_loop_run (f->result.loop);
  /* Not gonna make any particular assertions about what the error is. */
  g_assert_error (f->result.error, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD);

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
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpTestsContactListManager *manager;
  TpSimpleClientFactory *factory;
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
  tp_simple_client_factory_add_contact_features_varargs (factory,
      TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_DATA,
      TP_CONTACT_FEATURE_INVALID);

  /* Now put it online and wait for contact list state move to success */
  g_signal_connect (f->client_conn, "contact-list-changed",
      G_CALLBACK (contact_list_changed_cb), &got_contact_list_changed);
  g_signal_connect_swapped (f->client_conn, "notify::contact-list-state",
      G_CALLBACK (finish), &result);
  tp_cli_connection_call_connect (f->client_conn, -1, NULL, NULL, NULL, NULL);
  g_main_loop_run (result.loop);
  g_assert_no_error (result.error);

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

typedef struct
{
  Fixture *f;
  GMainLoop *loop;
  gboolean expecting_signal;
  gboolean got_stored;
  gboolean got_publish;
  gboolean got_subscribe;
} MembersChangedClosure;

static void
channel_prepared_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpChannel *channel = TP_CHANNEL (source_object);
  MembersChangedClosure *closure = user_data;
  const gchar *identifier;
  GError *error = NULL;

  tp_proxy_prepare_finish (channel, res, &error);
  g_assert_no_error (error);

  g_assert (closure->expecting_signal);

  /* We expect to just get the stored, publish and subscribe lists exactly
   * once */
  identifier = tp_channel_get_identifier (channel);
  if (g_strcmp0 (identifier, "stored") == 0)
    {
      g_assert (!closure->got_stored);
      closure->got_stored = TRUE;
    }
  else if (g_strcmp0 (identifier, "publish") == 0)
    {
      g_assert (!closure->got_publish);
      closure->got_publish = TRUE;
    }
  else if (g_strcmp0 (identifier, "subscribe") == 0)
    {
      g_assert (!closure->got_subscribe);
      closure->got_subscribe = TRUE;
    }
  else
    {
      g_assert_not_reached ();
    }

  if (closure->got_stored && closure->got_publish && closure->got_subscribe)
    g_main_loop_quit (closure->loop);
}

static DBusHandlerResult
message_filter (DBusConnection *connection,
    DBusMessage *msg,
    gpointer user_data)
{
  MembersChangedClosure *closure = user_data;

  if (dbus_message_is_signal (msg, TP_IFACE_CHANNEL_INTERFACE_GROUP,
      "MembersChanged"))
    {
      TpChannel *channel;
      DBusMessageIter iter, sub_iter;
      gint type;
      dbus_int32_t *added;
      gint n_added;

      channel = tp_channel_new (closure->f->client_conn,
          dbus_message_get_path (msg), TP_IFACE_CHANNEL_INTERFACE_GROUP,
          TP_HANDLE_TYPE_LIST, 0, NULL);
      tp_proxy_prepare_async (channel, NULL, channel_prepared_cb, closure);

      /* Extract the number of added handles */
      dbus_message_iter_init (msg, &iter);
      dbus_message_iter_next (&iter); /* Skipe the message */
      type = dbus_message_iter_get_arg_type (&iter);
      g_assert_cmpint (type, ==, DBUS_TYPE_ARRAY);
      dbus_message_iter_recurse (&iter, &sub_iter);
      type = dbus_message_iter_get_arg_type (&sub_iter);
      g_assert_cmpint (type, ==, DBUS_TYPE_UINT32);
      dbus_message_iter_get_fixed_array (&sub_iter, &added, &n_added);

      /* Bob, Alice and Carol == 3 */
      g_assert_cmpint (n_added, ==, 3);
    }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
test_initial_contact_list (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  const GQuark conn_features[] = { TP_CONNECTION_FEATURE_CONTACT_LIST, 0 };
  TpTestsContactListManager *manager;
  MembersChangedClosure closure;
  DBusConnection *dbus_connection;
  TpHandle alice;
  const gchar *alice_id = "alice";
  TpHandle bob_carol[2];
  const gchar *bob_id = "bob";
  const gchar *carol_id = "carol";

  manager = tp_tests_contacts_connection_get_contact_list_manager (
      f->service_conn);

  /* We use a filter to be sure not to miss the initial MembersChanged
   * signals */
  closure.f = f;
  closure.loop = g_main_loop_new (NULL, FALSE);
  closure.got_stored = FALSE;
  closure.got_publish = FALSE;
  closure.got_subscribe = FALSE;
  /* No signal is expected until after we connect */
  closure.expecting_signal = FALSE;

  dbus_connection = dbus_g_connection_get_connection (
      tp_proxy_get_dbus_connection (TP_PROXY (f->client_conn)));
  dbus_connection_ref (dbus_connection);
  dbus_connection_add_filter (dbus_connection, message_filter, &closure, NULL);
  dbus_bus_add_match (dbus_connection, MEMBERS_CHANGED_MATCH_RULE, NULL);

  /* Connection is OFFLINE initially */
  tp_tests_proxy_run_until_prepared (f->client_conn, conn_features);
  g_assert_cmpint (tp_connection_get_contact_list_state (f->client_conn), ==,
      TP_CONTACT_LIST_STATE_NONE);

  /* Add contacts in our initial roster CM-side */
  alice = tp_handle_ensure (f->service_repo, alice_id, NULL, NULL);
  tp_tests_contact_list_manager_add_initial_contacts (manager, 1, &alice);

  bob_carol[0] = tp_handle_ensure (f->service_repo, bob_id, NULL, NULL);
  bob_carol[1] = tp_handle_ensure (f->service_repo, carol_id, NULL, NULL);
  tp_tests_contact_list_manager_add_initial_contacts (manager, 2, bob_carol);

  /* Now put it online and wait for the contact list state to move to
   * success */
  closure.expecting_signal = TRUE;
  tp_cli_connection_call_connect (f->client_conn, -1,
      NULL, NULL, NULL, NULL);

  g_main_loop_run (closure.loop);

  dbus_bus_remove_match (dbus_connection, MEMBERS_CHANGED_MATCH_RULE, NULL);
  dbus_connection_remove_filter (dbus_connection, message_filter, &closure);
  dbus_connection_unref (dbus_connection);

  g_main_loop_unref (closure.loop);
}

static void
test_self_contact (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  const GQuark conn_features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };
  TpSimpleClientFactory *factory;
  TpContact *contact;

  factory = tp_proxy_get_factory (f->client_conn);
  tp_simple_client_factory_add_contact_features_varargs (factory,
      TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_INVALID);

  tp_cli_connection_call_connect (f->client_conn, -1, NULL, NULL, NULL, NULL);
  tp_tests_proxy_run_until_prepared (f->client_conn, conn_features);

  contact = tp_connection_get_self_contact (f->client_conn);
  g_assert (contact != NULL);
  g_assert (tp_contact_has_feature (contact, TP_CONTACT_FEATURE_ALIAS));
}

static void
setup_internal (Fixture *f,
    gboolean connect,
    gconstpointer user_data)
{
  tp_tests_create_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", connect, &f->base_connection, &f->client_conn);

  f->service_conn = TP_TESTS_CONTACTS_CONNECTION (f->base_connection);
  g_object_ref (f->service_conn);

  tp_tests_create_conn (TP_TESTS_TYPE_LEGACY_CONTACTS_CONNECTION,
      "me2@test.com", connect, &f->legacy_base_connection, &f->legacy_client_conn);

  tp_tests_create_conn (TP_TESTS_TYPE_NO_REQUESTS_CONNECTION,
      "me3@test.com", connect, &f->no_requests_base_connection,
      &f->no_requests_client_conn);

  f->service_repo = tp_base_connection_get_handles (f->base_connection,
      TP_HANDLE_TYPE_CONTACT);
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
    tp_tests_connection_assert_disconnect_succeeds (f->client_conn);

  tp_clear_object (&f->client_conn);
  f->service_repo = NULL;
  tp_clear_object (&f->service_conn);
  tp_clear_object (&f->base_connection);

  if (f->legacy_client_conn != NULL)
    tp_tests_connection_assert_disconnect_succeeds (f->legacy_client_conn);

  tp_clear_object (&f->legacy_client_conn);
  tp_clear_object (&f->legacy_base_connection);

  if (f->no_requests_client_conn != NULL)
    {
      tp_tests_connection_assert_disconnect_succeeds (
          f->no_requests_client_conn);
    }

  tp_clear_object (&f->no_requests_client_conn);
  tp_clear_object (&f->no_requests_base_connection);
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

  ADD (by_handle);
  ADD (by_handle_again);
  ADD (by_handle_upgrade);
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

  /* test if TpContact fallbacks to connection's capabilities if
   * ContactCapabilities is not implemented. */
  ADD (capabilities_without_contact_caps);

  /* test if TP_CONTACT_FEATURE_CAPABILITIES is prepared but with
   * an empty set of capabilities if the connection doesn't support
   * ContactCapabilities and Requests. */
  ADD (prepare_contact_caps_without_request);

  ADD (no_location);

  g_test_add ("/contacts/superfluous-attributes", Fixture, NULL,
      setup_broken_client_types_conn, test_superfluous_attributes,
      teardown);

  g_test_add ("/contacts/contact-list", Fixture, NULL,
      setup_no_connect, test_contact_list, teardown);

  g_test_add ("/contacts/initial-contact-list", Fixture, NULL,
      setup_no_connect, test_initial_contact_list, teardown);

  g_test_add ("/contacts/self-contact", Fixture, NULL,
      setup_no_connect, test_self_contact, teardown);

  ret = g_test_run ();

  g_assert (haze_remove_directory (dir));
  g_free (dir);

  return ret;
}
