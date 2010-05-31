/* A ContactList channel with handle type LIST or GROUP.
 *
 * Copyright © 2009-2010 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include <config.h>
#include <telepathy-glib/contact-list-channel-internal.h>

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/exportable-channel.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-generic.h>

#include <telepathy-glib/contact-list-manager-internal.h>

static void channel_iface_init (TpSvcChannelClass *iface);
static void list_channel_iface_init (TpSvcChannelClass *iface);
static void group_channel_iface_init (TpSvcChannelClass *iface);

/* Abstract base class */
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TpBaseContactListChannel,
    _tp_base_contact_list_channel,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL, channel_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_CONTACT_LIST, NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_GROUP,
      tp_group_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_EXPORTABLE_CHANNEL, NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_IFACE, NULL))

/* Subclass for handle type LIST */
G_DEFINE_TYPE_WITH_CODE (TpContactListChannel, _tp_contact_list_channel,
    TP_TYPE_BASE_CONTACT_LIST_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL, list_channel_iface_init))

/* Subclass for handle type GROUP */
G_DEFINE_TYPE_WITH_CODE (TpContactGroupChannel, _tp_contact_group_channel,
    TP_TYPE_BASE_CONTACT_LIST_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL, group_channel_iface_init))

static const gchar *contact_list_interfaces[] = {
    TP_IFACE_CHANNEL_INTERFACE_GROUP,
    NULL
};

enum
{
  PROP_OBJECT_PATH = 1,
  PROP_CHANNEL_TYPE,
  PROP_HANDLE_TYPE,
  PROP_HANDLE,
  PROP_TARGET_ID,
  PROP_REQUESTED,
  PROP_INITIATOR_HANDLE,
  PROP_INITIATOR_ID,
  PROP_CONNECTION,
  PROP_MANAGER,
  PROP_INTERFACES,
  PROP_CHANNEL_DESTROYED,
  PROP_CHANNEL_PROPERTIES,
  N_PROPS
};

struct _ExampleContactListBasePrivate
{
};

static void
_tp_base_contact_list_channel_init (TpBaseContactListChannel *self)
{
}

static void
_tp_contact_list_channel_init (TpContactListChannel *self)
{
}

static void
_tp_contact_group_channel_init (TpContactGroupChannel *self)
{
}

static void
tp_base_contact_list_channel_constructed (GObject *object)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) _tp_base_contact_list_channel_parent_class)->constructed;
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (self->conn,
      TP_HANDLE_TYPE_CONTACT);
  TpHandle self_handle = self->conn->self_handle;
  TpHandleRepoIface *handle_repo = tp_base_connection_get_handles (self->conn,
      self->handle_type);

  if (chain_up != NULL)
    chain_up (object);

  g_assert (TP_IS_BASE_CONNECTION (self->conn));
  g_assert (TP_IS_CONTACT_LIST_MANAGER (self->manager));

  tp_dbus_daemon_register_object (
      tp_base_connection_get_dbus_daemon (self->conn),
      self->object_path, self);

  tp_handle_ref (handle_repo, self->handle);
  tp_group_mixin_init (object, G_STRUCT_OFFSET (TpBaseContactListChannel,
        group), contact_repo, self_handle);
  /* Both the subclasses have full support for telepathy-spec 0.17.6. */
  tp_group_mixin_change_flags (object,
      TP_CHANNEL_GROUP_FLAG_PROPERTIES, 0);
}

static void
tp_contact_list_channel_constructed (GObject *object)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) _tp_contact_list_channel_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->handle_type == TP_HANDLE_TYPE_LIST);

  tp_group_mixin_change_flags (object,
      _tp_contact_list_manager_get_list_flags (self->manager, self->handle),
      0);
}

static void
tp_contact_group_channel_constructed (GObject *object)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) _tp_contact_group_channel_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->handle_type == TP_HANDLE_TYPE_GROUP);

  tp_group_mixin_change_flags (object,
      _tp_contact_list_manager_get_group_flags (self->manager), 0);
}


static void
tp_base_contact_list_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);

  switch (property_id)
    {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, self->object_path);
      break;

    case PROP_CHANNEL_TYPE:
      g_value_set_static_string (value, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST);
      break;

    case PROP_HANDLE_TYPE:
      g_value_set_uint (value, self->handle_type);
      break;

    case PROP_HANDLE:
      g_value_set_uint (value, self->handle);
      break;

    case PROP_TARGET_ID:
        {
          TpHandleRepoIface *handle_repo;

          if (self->conn == NULL)
            {
              g_value_set_string (value, "");
              break;
            }

          handle_repo = tp_base_connection_get_handles (
              self->conn, self->handle_type);

          g_value_set_string (value,
              tp_handle_inspect (handle_repo, self->handle));
        }
      break;

    case PROP_REQUESTED:
      g_value_set_boolean (value, FALSE);
      break;

    case PROP_INITIATOR_HANDLE:
      /* nobody initiates the Spanish Inquisition */
      g_value_set_uint (value, 0);
      break;

    case PROP_INITIATOR_ID:
      g_value_set_static_string (value, "");
      break;

    case PROP_CONNECTION:
      g_value_set_object (value, self->conn);
      break;

    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    case PROP_INTERFACES:
      g_value_set_boxed (value, contact_list_interfaces);
      break;

    case PROP_CHANNEL_DESTROYED:
      g_value_set_boolean (value, (self->conn == NULL));
      break;

    case PROP_CHANNEL_PROPERTIES:
      g_value_take_boxed (value,
          tp_dbus_properties_mixin_make_properties_hash (object,
              TP_IFACE_CHANNEL, "ChannelType",
              TP_IFACE_CHANNEL, "TargetHandleType",
              TP_IFACE_CHANNEL, "TargetHandle",
              TP_IFACE_CHANNEL, "TargetID",
              TP_IFACE_CHANNEL, "InitiatorHandle",
              TP_IFACE_CHANNEL, "InitiatorID",
              TP_IFACE_CHANNEL, "Requested",
              TP_IFACE_CHANNEL, "Interfaces",
              NULL));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_base_contact_list_channel_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);

  switch (property_id)
    {
    case PROP_OBJECT_PATH:
      g_free (self->object_path);
      self->object_path = g_value_dup_string (value);
      break;

    case PROP_HANDLE:
      /* we don't ref it here because we don't necessarily have access to the
       * repository (or even type) yet - instead we ref it in the constructor.
       */
      self->handle = g_value_get_uint (value);
      break;

    case PROP_HANDLE_TYPE:
      self->handle_type = g_value_get_uint (value);
      break;

    case PROP_CHANNEL_TYPE:
      /* this property is writable in the interface, but not actually
       * meaningfully changable on this channel, so we do nothing */
      break;

    case PROP_CONNECTION:
      g_assert (self->conn == NULL);      /* construct-only */
      self->conn = g_value_dup_object (value);
      break;

    case PROP_MANAGER:
      g_assert (self->manager == NULL);   /* construct-only */
      self->manager = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void
_tp_base_contact_list_channel_close (TpBaseContactListChannel *self)
{
  TpHandleRepoIface *handle_repo;

  if (self->conn == NULL)
    return;

  tp_svc_channel_emit_closed (self);

  tp_dbus_daemon_unregister_object (
      tp_base_connection_get_dbus_daemon (self->conn), self);

  handle_repo = tp_base_connection_get_handles (
      self->conn, self->handle_type);

  tp_handle_unref (handle_repo, self->handle);
  tp_group_mixin_finalize ((GObject *) self);

  g_object_unref (self->manager);
  self->manager = NULL;

  g_object_unref (self->conn);
  self->conn = NULL;
}

static void
tp_base_contact_list_channel_dispose (GObject *object)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (_tp_base_contact_list_channel_parent_class)->dispose;

  _tp_base_contact_list_channel_close (self);

  if (dispose != NULL)
    dispose (object);
}

static void
tp_base_contact_list_channel_finalize (GObject *object)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);
  void (*finalize) (GObject *) =
    G_OBJECT_CLASS (_tp_base_contact_list_channel_parent_class)->finalize;

  g_free (self->object_path);

  if (finalize != NULL)
    finalize (object);
}

static gboolean
tp_base_contact_list_channel_check_still_usable (
    TpBaseContactListChannel *self,
    GError **error)
{
  if (self->conn == NULL)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_TERMINATED,
          "Channel already closed");
      return FALSE;
    }

  return TRUE;
}

static gboolean
group_add_member (GObject *object,
    TpHandle handle,
    const gchar *message,
    GError **error)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);

  return tp_base_contact_list_channel_check_still_usable (self, error) &&
    _tp_contact_list_manager_add_to_group (self->manager,
      self->handle, handle, message, error);
}

static gboolean
group_remove_member (GObject *object,
    TpHandle handle,
    const gchar *message,
    GError **error)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);

  return tp_base_contact_list_channel_check_still_usable (self, error) &&
    _tp_contact_list_manager_remove_from_group (self->manager,
      self->handle, handle, message, error);
}

static gboolean
list_add_member (GObject *object,
    TpHandle handle,
    const gchar *message,
    GError **error)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);

  return tp_base_contact_list_channel_check_still_usable (self, error) &&
    _tp_contact_list_manager_add_to_list (self->manager,
      self->handle, handle, message, error);
}

static gboolean
list_remove_member (GObject *object,
    TpHandle handle,
    const gchar *message,
    GError **error)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (object);

  return tp_base_contact_list_channel_check_still_usable (self, error) &&
    _tp_contact_list_manager_remove_from_list (self->manager,
      self->handle, handle, message, error);
}

static void
_tp_base_contact_list_channel_class_init (TpBaseContactListChannelClass *cls)
{
  static TpDBusPropertiesMixinPropImpl channel_props[] = {
      { "TargetHandleType", "handle-type", NULL },
      { "TargetHandle", "handle", NULL },
      { "ChannelType", "channel-type", NULL },
      { "Interfaces", "interfaces", NULL },
      { "TargetID", "target-id", NULL },
      { "Requested", "requested", NULL },
      { "InitiatorHandle", "initiator-handle", NULL },
      { "InitiatorID", "initiator-id", NULL },
      { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
      { TP_IFACE_CHANNEL,
        tp_dbus_properties_mixin_getter_gobject_properties,
        NULL,
        channel_props,
      },
      { NULL }
  };
  GObjectClass *object_class = (GObjectClass *) cls;

  object_class->constructed = tp_base_contact_list_channel_constructed;
  object_class->set_property = tp_base_contact_list_channel_set_property;
  object_class->get_property = tp_base_contact_list_channel_get_property;
  object_class->dispose = tp_base_contact_list_channel_dispose;
  object_class->finalize = tp_base_contact_list_channel_finalize;

  g_object_class_override_property (object_class, PROP_OBJECT_PATH,
      "object-path");
  g_object_class_override_property (object_class, PROP_CHANNEL_TYPE,
      "channel-type");
  g_object_class_override_property (object_class, PROP_HANDLE_TYPE,
      "handle-type");
  g_object_class_override_property (object_class, PROP_HANDLE, "handle");

  g_object_class_override_property (object_class, PROP_CHANNEL_DESTROYED,
      "channel-destroyed");
  g_object_class_override_property (object_class, PROP_CHANNEL_PROPERTIES,
      "channel-properties");

  g_object_class_install_property (object_class, PROP_CONNECTION,
      g_param_spec_object ("connection", "TpBaseConnection object",
        "Connection object that owns this channel",
        TP_TYPE_BASE_CONNECTION,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MANAGER,
      g_param_spec_object ("manager", "TpContactListManager",
        "TpContactListManager object that owns this channel",
        TP_TYPE_CONTACT_LIST_MANAGER,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_INTERFACES,
      g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
        "Additional Channel.Interface.* interfaces",
        G_TYPE_STRV,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_TARGET_ID,
      g_param_spec_string ("target-id", "List's ID",
        "The string obtained by inspecting the list's handle",
        NULL,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_INITIATOR_HANDLE,
      g_param_spec_uint ("initiator-handle", "Initiator's handle",
        "The contact who initiated the channel",
        0, G_MAXUINT32, 0,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_INITIATOR_ID,
      g_param_spec_string ("initiator-id", "Initiator's ID",
        "The string obtained by inspecting the initiator-handle",
        NULL,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_REQUESTED,
      g_param_spec_boolean ("requested", "Requested?",
        "True if this channel was requested by the local user",
        FALSE,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  cls->dbus_properties_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpBaseContactListChannelClass, dbus_properties_class));

  /* Group mixin is initialized separately for each subclass - they have
   *  different callbacks */
}

static void
_tp_contact_list_channel_class_init (TpContactListChannelClass *cls)
{
  GObjectClass *object_class = (GObjectClass *) cls;

  object_class->constructed = tp_contact_list_channel_constructed;

  tp_group_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpBaseContactListChannelClass, group_class),
      list_add_member,
      list_remove_member);
  tp_group_mixin_init_dbus_properties (object_class);
}

static void
_tp_contact_group_channel_class_init (TpContactGroupChannelClass *cls)
{
  GObjectClass *object_class = (GObjectClass *) cls;

  object_class->constructed = tp_contact_group_channel_constructed;

  tp_group_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpBaseContactListChannelClass, group_class),
      group_add_member,
      group_remove_member);
  tp_group_mixin_init_dbus_properties (object_class);
}

static void
list_channel_close (TpSvcChannel *iface G_GNUC_UNUSED,
    DBusGMethodInvocation *context)
{
  GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "ContactList channels with handle type LIST may not be closed" };

  dbus_g_method_return_error (context, &e);
}

static void
group_channel_close (TpSvcChannel *iface,
    DBusGMethodInvocation *context)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (iface);
  GError *error = NULL;

  if (!tp_base_contact_list_channel_check_still_usable (self, &error))
    goto error;

  if (tp_handle_set_size (self->group.members) > 0)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Non-empty groups may not be deleted (closed)");
      goto error;
    }

  if (!_tp_contact_list_manager_delete_group_by_handle (self->manager,
      self->handle, &error))
    goto error;

  tp_svc_channel_return_from_close (context);
  return;

error:
  dbus_g_method_return_error (context, error);
  g_clear_error (&error);
}

static void
channel_get_channel_type (TpSvcChannel *iface G_GNUC_UNUSED,
    DBusGMethodInvocation *context)
{
  tp_svc_channel_return_from_get_channel_type (context,
      TP_IFACE_CHANNEL_TYPE_CONTACT_LIST);
}

static void
channel_get_handle (TpSvcChannel *iface,
    DBusGMethodInvocation *context)
{
  TpBaseContactListChannel *self = TP_BASE_CONTACT_LIST_CHANNEL (iface);

  tp_svc_channel_return_from_get_handle (context, self->handle_type,
      self->handle);
}

static void
channel_get_interfaces (TpSvcChannel *iface G_GNUC_UNUSED,
    DBusGMethodInvocation *context)
{
  tp_svc_channel_return_from_get_interfaces (context,
      contact_list_interfaces);
}

static void
channel_iface_init (TpSvcChannelClass *iface)
{
#define IMPLEMENT(x) tp_svc_channel_implement_##x (iface, channel_##x)
  /* close is implemented in subclasses, so don't IMPLEMENT (close); */
  IMPLEMENT (get_channel_type);
  IMPLEMENT (get_handle);
  IMPLEMENT (get_interfaces);
#undef IMPLEMENT
}

static void
list_channel_iface_init (TpSvcChannelClass *iface)
{
#define IMPLEMENT(x) tp_svc_channel_implement_##x (iface, list_channel_##x)
  IMPLEMENT (close);
#undef IMPLEMENT
}

static void
group_channel_iface_init (TpSvcChannelClass *iface)
{
#define IMPLEMENT(x) tp_svc_channel_implement_##x (iface, group_channel_##x)
  IMPLEMENT (close);
#undef IMPLEMENT
}
