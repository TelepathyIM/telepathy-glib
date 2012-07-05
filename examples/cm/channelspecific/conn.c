/*
 * conn.c - an example connection
 *
 * Copyright © 2007-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "conn.h"

#include <string.h>

#include <dbus/dbus-glib.h>

#include <telepathy-glib/telepathy-glib.h>

#include "protocol.h"
#include "room-manager.h"

G_DEFINE_TYPE_WITH_CODE (ExampleCSHConnection,
    example_csh_connection,
    TP_TYPE_BASE_CONNECTION,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACTS,
      tp_contacts_mixin_iface_init))

/* type definition stuff */

enum
{
  PROP_ACCOUNT = 1,
  PROP_SIMULATION_DELAY,
  N_PROPS
};

struct _ExampleCSHConnectionPrivate
{
  gchar *account;
  guint simulation_delay;
};

static void
example_csh_connection_init (ExampleCSHConnection *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_CSH_CONNECTION, ExampleCSHConnectionPrivate);
}

static void
get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *spec)
{
  ExampleCSHConnection *self = EXAMPLE_CSH_CONNECTION (object);

  switch (property_id) {
    case PROP_ACCOUNT:
      g_value_set_string (value, self->priv->account);
      break;

    case PROP_SIMULATION_DELAY:
      g_value_set_uint (value, self->priv->simulation_delay);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
  }
}

static void
set_property (GObject *object,
              guint property_id,
              const GValue *value,
              GParamSpec *spec)
{
  ExampleCSHConnection *self = EXAMPLE_CSH_CONNECTION (object);

  switch (property_id) {
    case PROP_ACCOUNT:
      g_free (self->priv->account);
      self->priv->account = g_utf8_strdown (g_value_get_string (value), -1);
      break;

    case PROP_SIMULATION_DELAY:
      self->priv->simulation_delay = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
  }
}

static void
finalize (GObject *object)
{
  ExampleCSHConnection *self = EXAMPLE_CSH_CONNECTION (object);

  tp_contacts_mixin_finalize (object);
  g_free (self->priv->account);

  G_OBJECT_CLASS (example_csh_connection_parent_class)->finalize (object);
}

static gchar *
get_unique_connection_name (TpBaseConnection *conn)
{
  ExampleCSHConnection *self = EXAMPLE_CSH_CONNECTION (conn);

  return g_strdup (self->priv->account);
}

static gchar *
example_csh_normalize_contact (TpHandleRepoIface *repo G_GNUC_UNUSED,
                               const gchar *id,
                               gpointer context G_GNUC_UNUSED,
                               GError **error)
{
  gchar *normal = NULL;

  if (example_csh_protocol_check_contact_id (id, &normal, error))
    return normal;
  else
    return NULL;
}

static gchar *
example_csh_normalize_room (TpHandleRepoIface *repo,
                            const gchar *id,
                            gpointer context,
                            GError **error)
{
  /* See example_csh_protocol_normalize_contact() for syntax. */

  if (id[0] != '#')
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_HANDLE,
          "Chatroom names in this protocol start with #");
    }

  if (id[1] == '\0')
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_HANDLE,
          "Chatroom name cannot be empty");
      return NULL;
    }

  if (strchr (id, '@') != NULL)
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_HANDLE,
          "Chatroom names in this protocol cannot contain '@'");
      return NULL;
    }

  return g_utf8_normalize (id, -1, G_NORMALIZE_ALL_COMPOSE);
}

static void
create_handle_repos (TpBaseConnection *conn,
                     TpHandleRepoIface *repos[TP_NUM_HANDLE_TYPES])
{
  repos[TP_HANDLE_TYPE_CONTACT] = tp_dynamic_handle_repo_new
      (TP_HANDLE_TYPE_CONTACT, example_csh_normalize_contact, NULL);

  repos[TP_HANDLE_TYPE_ROOM] = tp_dynamic_handle_repo_new
      (TP_HANDLE_TYPE_ROOM, example_csh_normalize_room, NULL);
}

static GPtrArray *
create_channel_managers (TpBaseConnection *conn)
{
  ExampleCSHConnection *self = EXAMPLE_CSH_CONNECTION (conn);
  GPtrArray *ret = g_ptr_array_sized_new (1);

  g_ptr_array_add (ret, g_object_new (EXAMPLE_TYPE_CSH_ROOM_MANAGER,
        "connection", conn,
        "simulation-delay", self->priv->simulation_delay,
        NULL));

  return ret;
}

static gboolean
start_connecting (TpBaseConnection *conn,
                  GError **error)
{
  ExampleCSHConnection *self = EXAMPLE_CSH_CONNECTION (conn);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (conn,
      TP_HANDLE_TYPE_CONTACT);
  TpHandle self_handle;

  /* In a real connection manager we'd ask the underlying implementation to
   * start connecting, then go to state CONNECTED when finished, but here
   * we can do it immediately. */

  self_handle = tp_handle_ensure (contact_repo, self->priv->account,
      NULL, error);

  if (self_handle == 0)
    return FALSE;

  tp_base_connection_set_self_handle (conn, self_handle);

  tp_base_connection_change_status (conn, TP_CONNECTION_STATUS_CONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);

  return TRUE;
}

static void
shut_down (TpBaseConnection *conn)
{
  /* In a real connection manager we'd ask the underlying implementation to
   * start shutting down, then call this function when finished, but here
   * we can do it immediately. */
  tp_base_connection_finish_shutdown (conn);
}

static void
constructed (GObject *object)
{
  TpBaseConnection *base = TP_BASE_CONNECTION (object);
  void (*chain_up) (GObject *) =
    G_OBJECT_CLASS (example_csh_connection_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  tp_contacts_mixin_init (object,
      G_STRUCT_OFFSET (ExampleCSHConnection, contacts_mixin));
  tp_base_connection_register_with_contacts_mixin (base);
}

static const gchar *interfaces_always_present[] = {
    TP_IFACE_CONNECTION_INTERFACE_REQUESTS,
    TP_IFACE_CONNECTION_INTERFACE_CONTACTS,
    NULL };

const gchar * const *
example_csh_connection_get_possible_interfaces (void)
{
  /* in this example CM we don't have any extra interfaces that are sometimes,
   * but not always, present */
  return interfaces_always_present;
}

static GPtrArray *
get_interfaces_always_present (TpBaseConnection *base)
{
  GPtrArray *interfaces;
  guint i;

  interfaces = TP_BASE_CONNECTION_CLASS (
      example_csh_connection_parent_class)->get_interfaces_always_present (base);

  for (i = 0; interfaces_always_present[i] != NULL; i++)
    g_ptr_array_add (interfaces, (gchar *) interfaces_always_present[i]);

  return interfaces;
}

static void
example_csh_connection_class_init (ExampleCSHConnectionClass *klass)
{
  TpBaseConnectionClass *base_class =
      (TpBaseConnectionClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  object_class->constructed = constructed;
  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->finalize = finalize;
  g_type_class_add_private (klass, sizeof (ExampleCSHConnectionPrivate));

  base_class->create_handle_repos = create_handle_repos;
  base_class->get_unique_connection_name = get_unique_connection_name;
  base_class->create_channel_managers = create_channel_managers;
  base_class->start_connecting = start_connecting;
  base_class->shut_down = shut_down;
  base_class->get_interfaces_always_present = get_interfaces_always_present;

  param_spec = g_param_spec_string ("account", "Account name",
      "The username of this user", NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT, param_spec);

  param_spec = g_param_spec_uint ("simulation-delay", "Simulation delay",
      "Delay between simulated network events",
      0, G_MAXUINT32, 500,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SIMULATION_DELAY,
      param_spec);

  tp_contacts_mixin_class_init (object_class,
      G_STRUCT_OFFSET (ExampleCSHConnectionClass, contacts_mixin));
}
