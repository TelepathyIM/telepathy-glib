/*
 * conn.c - an example connection
 *
 * Copyright © 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include "conn.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

#include "contact-list.h"
#include "protocol.h"

static void init_aliasing (gpointer, gpointer);
static void init_presence (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (ExampleContactListConnection,
    example_contact_list_connection,
    TP_TYPE_BASE_CONNECTION,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_ALIASING1,
      init_aliasing)
    G_IMPLEMENT_INTERFACE (TP_TYPE_PRESENCE_MIXIN, init_presence)
    )

enum
{
  PROP_ACCOUNT = 1,
  PROP_SIMULATION_DELAY,
  N_PROPS
};

struct _ExampleContactListConnectionPrivate
{
  gchar *account;
  guint simulation_delay;
  ExampleContactList *contact_list;
  gboolean away;
};

static void
example_contact_list_connection_init (ExampleContactListConnection *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_CONTACT_LIST_CONNECTION,
      ExampleContactListConnectionPrivate);
}

static void
get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *spec)
{
  ExampleContactListConnection *self =
    EXAMPLE_CONTACT_LIST_CONNECTION (object);

  switch (property_id)
    {
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
  ExampleContactListConnection *self =
    EXAMPLE_CONTACT_LIST_CONNECTION (object);

  switch (property_id)
    {
    case PROP_ACCOUNT:
      g_free (self->priv->account);
      self->priv->account = g_value_dup_string (value);
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
  ExampleContactListConnection *self =
    EXAMPLE_CONTACT_LIST_CONNECTION (object);

  g_free (self->priv->account);

  G_OBJECT_CLASS (example_contact_list_connection_parent_class)->finalize (
      object);
}

static gchar *
get_unique_connection_name (TpBaseConnection *conn)
{
  ExampleContactListConnection *self = EXAMPLE_CONTACT_LIST_CONNECTION (conn);

  return g_strdup_printf ("%s@%p", self->priv->account, self);
}

gchar *
example_contact_list_normalize_contact (TpHandleRepoIface *repo,
                                        const gchar *id,
                                        gpointer context,
                                        GError **error)
{
  gchar *normal = NULL;

  if (example_contact_list_protocol_check_contact_id (id, &normal, error))
    return normal;
  else
    return NULL;
}

static void
create_handle_repos (TpBaseConnection *conn,
                     TpHandleRepoIface *repos[TP_NUM_ENTITY_TYPES])
{
  repos[TP_ENTITY_TYPE_CONTACT] = tp_dynamic_handle_repo_new
      (TP_ENTITY_TYPE_CONTACT, example_contact_list_normalize_contact, NULL);
}

static void
alias_updated_cb (ExampleContactList *contact_list,
                  TpHandle contact,
                  ExampleContactListConnection *self)
{
  GHashTable *aliases;

  aliases = g_hash_table_new (NULL, NULL);
  g_hash_table_insert (aliases, GUINT_TO_POINTER (contact),
      (gpointer) example_contact_list_get_alias (contact_list, contact));

  tp_svc_connection_interface_aliasing1_emit_aliases_changed (self, aliases);

  g_hash_table_unref (aliases);
}

static void
presence_updated_cb (ExampleContactList *contact_list,
                     TpHandle contact,
                     ExampleContactListConnection *self)
{
  TpBaseConnection *base = (TpBaseConnection *) self;
  TpPresenceStatus *status;

  /* we ignore the presence indicated by the contact list for our own handle */
  if (contact == tp_base_connection_get_self_handle (base))
    return;

  status = tp_presence_status_new (
      example_contact_list_get_presence (contact_list, contact),
      NULL);
  tp_presence_mixin_emit_one_presence_update (base, contact, status);
  tp_presence_status_free (status);
}

static GPtrArray *
create_channel_managers (TpBaseConnection *conn)
{
  return g_ptr_array_sized_new (0);
}

static gboolean
start_connecting (TpBaseConnection *conn,
                  GError **error)
{
  ExampleContactListConnection *self = EXAMPLE_CONTACT_LIST_CONNECTION (conn);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (conn,
      TP_ENTITY_TYPE_CONTACT);
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
example_contact_list_connection_fill_contact_attributes (TpBaseConnection *conn,
    const gchar *dbus_interface,
    TpHandle contact,
    GVariantDict *attributes)
{
  ExampleContactListConnection *self =
    EXAMPLE_CONTACT_LIST_CONNECTION (conn);

  if (!tp_strdiff (dbus_interface, TP_IFACE_CONNECTION_INTERFACE_ALIASING1))
    {
      g_variant_dict_insert (attributes,
          TP_TOKEN_CONNECTION_INTERFACE_ALIASING1_ALIAS, "s",
          example_contact_list_get_alias (self->priv->contact_list, contact));
      return;
    }

  if (tp_base_contact_list_fill_contact_attributes (
        TP_BASE_CONTACT_LIST (self->priv->contact_list),
        dbus_interface, contact, attributes))
    return;

  if (tp_presence_mixin_fill_contact_attributes (conn,
        dbus_interface, contact, attributes))
    return;

  ((TpBaseConnectionClass *) example_contact_list_connection_parent_class)->
    fill_contact_attributes (conn, dbus_interface, contact, attributes);
}

static void
constructed (GObject *object)
{
  ExampleContactListConnection *self = EXAMPLE_CONTACT_LIST_CONNECTION (object);
  GDBusObjectSkeleton *skel = G_DBUS_OBJECT_SKELETON (object);
  GDBusInterfaceSkeleton *iface;
  void (*chain_up) (GObject *) =
    G_OBJECT_CLASS (example_contact_list_connection_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  self->priv->contact_list = EXAMPLE_CONTACT_LIST (g_object_new (
          EXAMPLE_TYPE_CONTACT_LIST,
          "connection", self,
          "simulation-delay", self->priv->simulation_delay,
          NULL));

  g_signal_connect (self->priv->contact_list, "alias-updated",
      G_CALLBACK (alias_updated_cb), self);
  g_signal_connect (self->priv->contact_list, "presence-updated",
      G_CALLBACK (presence_updated_cb), self);

  tp_presence_mixin_init (TP_BASE_CONNECTION (object));

  iface = tp_svc_interface_skeleton_new (skel,
      TP_TYPE_SVC_CONNECTION_INTERFACE_ALIASING1);
  g_dbus_object_skeleton_add_interface (skel, iface);
  g_object_unref (iface);
}

static gboolean
status_available (TpBaseConnection *base,
    guint index_)
{
  return tp_base_connection_check_connected (base, NULL);
}

static TpPresenceStatus *
get_contact_status (TpBaseConnection *base,
    TpHandle contact)
{
  ExampleContactListConnection *self = EXAMPLE_CONTACT_LIST_CONNECTION (base);
  ExampleContactListPresence presence;

  /* we get our own status from the connection, and everyone else's status
   * from the contact lists */
  if (contact == tp_base_connection_get_self_handle (base))
    {
      presence = (self->priv->away ? EXAMPLE_CONTACT_LIST_PRESENCE_AWAY
          : EXAMPLE_CONTACT_LIST_PRESENCE_AVAILABLE);
    }
  else
    {
      presence = example_contact_list_get_presence (
          self->priv->contact_list, contact);
    }

  return tp_presence_status_new (presence, "");
}

static gboolean
set_own_status (TpBaseConnection *base,
                const TpPresenceStatus *status,
                GError **error)
{
  ExampleContactListConnection *self = EXAMPLE_CONTACT_LIST_CONNECTION (base);
  GHashTable *presences;

  if (status->index == EXAMPLE_CONTACT_LIST_PRESENCE_AWAY)
    {
      if (self->priv->away)
        return TRUE;

      self->priv->away = TRUE;
    }
  else
    {
      if (!self->priv->away)
        return TRUE;

      self->priv->away = FALSE;
    }

  presences = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, NULL);
  g_hash_table_insert (presences,
      GUINT_TO_POINTER (tp_base_connection_get_self_handle (base)),
      (gpointer) status);
  tp_presence_mixin_emit_presence_update (base, presences);
  g_hash_table_unref (presences);
  return TRUE;
}

static void
init_presence (gpointer g_iface,
    gpointer iface_data)
{
  TpPresenceMixinInterface *iface = g_iface;

  iface->status_available = status_available;
  iface->get_contact_status = get_contact_status;
  iface->set_own_status = set_own_status;
  iface->statuses = example_contact_list_presence_statuses ();
}


static const gchar *interfaces_always_present[] = {
    TP_IFACE_CONNECTION_INTERFACE_ALIASING1,
    TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST1,
    TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUPS1,
    TP_IFACE_CONNECTION_INTERFACE_CONTACT_BLOCKING1,
    TP_IFACE_CONNECTION_INTERFACE_PRESENCE1,
    NULL };

const gchar * const *
example_contact_list_connection_get_possible_interfaces (void)
{
  /* in this example CM we don't have any extra interfaces that are sometimes,
   * but not always, present */
  return interfaces_always_present;
}

enum
{
  ALIASING_DP_ALIAS_FLAGS,
};

static void
aliasing_get_dbus_property (GObject *object,
    GQuark interface,
    GQuark name,
    GValue *value,
    gpointer user_data)
{
  switch (GPOINTER_TO_UINT (user_data))
    {
    case ALIASING_DP_ALIAS_FLAGS:
      g_value_set_uint (value, TP_CONNECTION_ALIAS_FLAG_USER_SET);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
example_contact_list_connection_class_init (
    ExampleContactListConnectionClass *klass)
{
  TpBaseConnectionClass *base_class = (TpBaseConnectionClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;
  static TpDBusPropertiesMixinPropImpl aliasing_props[] = {
    { "AliasFlags", GUINT_TO_POINTER (ALIASING_DP_ALIAS_FLAGS), NULL },
    { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
        { TP_IFACE_CONNECTION_INTERFACE_ALIASING1,
          aliasing_get_dbus_property,
          NULL,
          aliasing_props,
        },
        { NULL }
  };

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->finalize = finalize;
  g_type_class_add_private (klass,
      sizeof (ExampleContactListConnectionPrivate));

  base_class->create_handle_repos = create_handle_repos;
  base_class->get_unique_connection_name = get_unique_connection_name;
  base_class->create_channel_managers = create_channel_managers;
  base_class->start_connecting = start_connecting;
  base_class->shut_down = shut_down;
  base_class->fill_contact_attributes =
    example_contact_list_connection_fill_contact_attributes;

  param_spec = g_param_spec_string ("account", "Account name",
      "The username of this user", NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT, param_spec);

  param_spec = g_param_spec_uint ("simulation-delay", "Simulation delay",
      "Delay between simulated network events",
      0, G_MAXUINT32, 1000,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SIMULATION_DELAY,
      param_spec);

  klass->properties_mixin.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (ExampleContactListConnectionClass, properties_mixin));
}

static void
request_aliases (TpSvcConnectionInterfaceAliasing1 *aliasing,
                 const GArray *contacts,
                 GDBusMethodInvocation *context)
{
  ExampleContactListConnection *self =
    EXAMPLE_CONTACT_LIST_CONNECTION (aliasing);
  TpBaseConnection *base = TP_BASE_CONNECTION (aliasing);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (base,
      TP_ENTITY_TYPE_CONTACT);
  GPtrArray *result;
  gchar **strings;
  GError *error = NULL;
  guint i;

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (base, context);

  if (!tp_handles_are_valid (contact_repo, contacts, FALSE, &error))
    {
      g_dbus_method_invocation_return_gerror (context, error);
      g_error_free (error);
      return;
    }

  result = g_ptr_array_sized_new (contacts->len + 1);

  for (i = 0; i < contacts->len; i++)
    {
      TpHandle contact = g_array_index (contacts, TpHandle, i);
      const gchar *alias = example_contact_list_get_alias (
          self->priv->contact_list, contact);

      g_ptr_array_add (result, (gchar *) alias);
    }

  g_ptr_array_add (result, NULL);
  strings = (gchar **) g_ptr_array_free (result, FALSE);
  tp_svc_connection_interface_aliasing1_return_from_request_aliases (context,
      (const gchar **) strings);
  g_free (strings);
}

static void
set_aliases (TpSvcConnectionInterfaceAliasing1 *aliasing,
             GHashTable *aliases,
             GDBusMethodInvocation *context)
{
  ExampleContactListConnection *self =
    EXAMPLE_CONTACT_LIST_CONNECTION (aliasing);
  TpBaseConnection *base = TP_BASE_CONNECTION (aliasing);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (base,
      TP_ENTITY_TYPE_CONTACT);
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, aliases);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GError *error = NULL;

      if (!tp_handle_is_valid (contact_repo, GPOINTER_TO_UINT (key),
            &error))
        {
          g_dbus_method_invocation_return_gerror (context, error);
          g_error_free (error);
          return;
        }
    }

  g_hash_table_iter_init (&iter, aliases);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      example_contact_list_set_alias (self->priv->contact_list,
          GPOINTER_TO_UINT (key), value);
    }

  tp_svc_connection_interface_aliasing1_return_from_set_aliases (context);
}

static void
init_aliasing (gpointer iface,
               gpointer iface_data G_GNUC_UNUSED)
{
  TpSvcConnectionInterfaceAliasing1Class *klass = iface;

#define IMPLEMENT(x) tp_svc_connection_interface_aliasing1_implement_##x (\
    klass, x)
  IMPLEMENT(request_aliases);
  IMPLEMENT(set_aliases);
#undef IMPLEMENT
}
