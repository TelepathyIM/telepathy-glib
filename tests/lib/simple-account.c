/*
 * simple-account.c - a simple account service.
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "simple-account.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/svc-account.h>

static void account_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (TpTestsSimpleAccount,
    tp_tests_simple_account,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_ACCOUNT,
        account_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_ACCOUNT_INTERFACE_STORAGE,
        NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
        tp_dbus_properties_mixin_iface_init)
    )

/* TP_IFACE_ACCOUNT is implied */
static const char *ACCOUNT_INTERFACES[] = {
    TP_IFACE_ACCOUNT_INTERFACE_STORAGE,
    NULL };

enum
{
  PROP_0,
  PROP_INTERFACES,
  PROP_DISPLAY_NAME,
  PROP_ICON,
  PROP_VALID,
  PROP_ENABLED,
  PROP_NICKNAME,
  PROP_PARAMETERS,
  PROP_AUTOMATIC_PRESENCE,
  PROP_CONNECT_AUTO,
  PROP_CONNECTION,
  PROP_CONNECTION_STATUS,
  PROP_CONNECTION_STATUS_REASON,
  PROP_CURRENT_PRESENCE,
  PROP_REQUESTED_PRESENCE,
  PROP_NORMALIZED_NAME,
  PROP_HAS_BEEN_ONLINE,
  PROP_STORAGE_PROVIDER,
  PROP_STORAGE_IDENTIFIER,
  PROP_STORAGE_SPECIFIC_INFORMATION,
  PROP_STORAGE_RESTRICTIONS
};

struct _TpTestsSimpleAccountPrivate
{
  gpointer unused;
  GHashTable *parameters;
};

static void
account_iface_init (gpointer klass,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_account_implement_##x (\
  klass, tp_tests_simple_account_##x)
  /* TODO */
#undef IMPLEMENT
}


static void
tp_tests_simple_account_init (TpTestsSimpleAccount *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TESTS_TYPE_SIMPLE_ACCOUNT,
      TpTestsSimpleAccountPrivate);
  self->priv->parameters = g_hash_table_new_full (g_str_hash, g_str_equal,
      (GDestroyNotify) g_free, (GDestroyNotify) tp_g_value_slice_free);
}

static gpointer
_parameters_hash_copy_key (gpointer boxed)
{
  return g_strdup (boxed);
}

static gpointer
_parameters_hash_copy_value (gpointer boxed)
{
  GValue *ret = tp_g_value_slice_new (G_TYPE_STRING);

  g_value_copy (boxed, ret);
  return ret;
}


static void
tp_tests_simple_account_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpTestsSimpleAccount *self = TP_TESTS_SIMPLE_ACCOUNT (object);

  switch (param_id) {
      case PROP_PARAMETERS:
        tp_g_hash_table_update (self->priv->parameters,
            g_value_get_boxed (value), _parameters_hash_copy_key,
            _parameters_hash_copy_value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  }
}


static void
tp_tests_simple_account_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *spec)
{
  TpTestsSimpleAccount *self = TP_TESTS_SIMPLE_ACCOUNT (object);
  GValueArray *presence;
  GValue identifier = { 0, };

  presence = tp_value_array_build (3,
      G_TYPE_UINT, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE,
      G_TYPE_STRING, "available",
      G_TYPE_STRING, "",
      G_TYPE_INVALID);

  g_value_init (&identifier, G_TYPE_STRING);
  g_value_set_string (&identifier, "unique-identifier");

  switch (property_id) {
    case PROP_INTERFACES:
      g_value_set_boxed (value, ACCOUNT_INTERFACES);
      break;
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, "Fake Account");
      break;
    case PROP_ICON:
      g_value_set_string (value, "");
      break;
    case PROP_VALID:
      g_value_set_boolean (value, TRUE);
      break;
    case PROP_ENABLED:
      g_value_set_boolean (value, TRUE);
      break;
    case PROP_NICKNAME:
      g_value_set_string (value, "badger");
      break;
    case PROP_PARAMETERS:
      g_value_set_boxed (value, self->priv->parameters);
      break;
    case PROP_AUTOMATIC_PRESENCE:
      g_value_set_boxed (value, presence);
      break;
    case PROP_CONNECT_AUTO:
      g_value_set_boolean (value, FALSE);
      break;
    case PROP_CONNECTION:
      g_value_set_boxed (value, "/");
      break;
    case PROP_CONNECTION_STATUS:
      g_value_set_uint (value, TP_CONNECTION_STATUS_CONNECTED);
      break;
    case PROP_CONNECTION_STATUS_REASON:
      g_value_set_uint (value, TP_CONNECTION_STATUS_REASON_REQUESTED);
      break;
    case PROP_CURRENT_PRESENCE:
      g_value_set_boxed (value, presence);
      break;
    case PROP_REQUESTED_PRESENCE:
      g_value_set_boxed (value, presence);
      break;
    case PROP_NORMALIZED_NAME:
      g_value_set_string (value, "");
      break;
    case PROP_HAS_BEEN_ONLINE:
      g_value_set_boolean (value, TRUE);
      break;
    case PROP_STORAGE_PROVIDER:
      g_value_set_string (value, "org.freedesktop.Telepathy.glib.test");
      break;
    case PROP_STORAGE_IDENTIFIER:
      g_value_set_boxed (value, &identifier);
      break;
    case PROP_STORAGE_SPECIFIC_INFORMATION:
      g_value_take_boxed (value, tp_asv_new (
            "one", G_TYPE_INT, 1,
            "two", G_TYPE_UINT, 2,
            "marco", G_TYPE_STRING, "polo",
            NULL));
      break;
    case PROP_STORAGE_RESTRICTIONS:
      g_value_set_uint (value,
          TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_ENABLED |
          TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_PARAMETERS);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
      break;
  }

  g_boxed_free (TP_STRUCT_TYPE_SIMPLE_PRESENCE, presence);
  g_value_unset (&identifier);
}

/**
  * This class currently only provides the minimum for
  * tp_account_prepare to succeed. This turns out to be only a working
  * Properties.GetAll().
  */
static void
tp_tests_simple_account_class_init (TpTestsSimpleAccountClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  static TpDBusPropertiesMixinPropImpl a_props[] = {
        { "Interfaces", "interfaces", NULL },
        { "DisplayName", "display-name", NULL },
        { "Icon", "icon", NULL },
        { "Valid", "valid", NULL },
        { "Enabled", "enabled", NULL },
        { "Nickname", "nickname", NULL },
        { "Parameters", "parameters", NULL },
        { "AutomaticPresence", "automatic-presence", NULL },
        { "ConnectAutomatically", "connect-automatically", NULL },
        { "Connection", "connection", NULL },
        { "ConnectionStatus", "connection-status", NULL },
        { "ConnectionStatusReason", "connection-status-reason", NULL },
        { "CurrentPresence", "current-presence", NULL },
        { "RequestedPresence", "requested-presence", NULL },
        { "NormalizedName", "normalized-name", NULL },
        { "HasBeenOnline", "has-been-online", NULL },
        { NULL }
  };

  static TpDBusPropertiesMixinPropImpl ais_props[] = {
        { "StorageProvider", "storage-provider", NULL },
        { "StorageIdentifier", "storage-identifier", NULL },
        { "StorageSpecificInformation", "storage-specific-information", NULL },
        { "StorageRestrictions", "storage-restrictions", NULL },
        { NULL },
  };

  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
        { TP_IFACE_ACCOUNT,
          tp_dbus_properties_mixin_getter_gobject_properties,
          NULL,
          a_props
        },
        {
          TP_IFACE_ACCOUNT_INTERFACE_STORAGE,
          tp_dbus_properties_mixin_getter_gobject_properties,
          NULL,
          ais_props
        },
        { NULL },
  };

  g_type_class_add_private (klass, sizeof (TpTestsSimpleAccountPrivate));
  object_class->get_property = tp_tests_simple_account_get_property;
  object_class->set_property = tp_tests_simple_account_set_property;

  param_spec = g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
      "In this case we only implement Account, so none.",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);

  param_spec = g_param_spec_string ("display-name", "display name",
      "DisplayName property",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DISPLAY_NAME, param_spec);

  param_spec = g_param_spec_string ("icon", "icon",
      "Icon property",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ICON, param_spec);

  param_spec = g_param_spec_boolean ("valid", "valid",
      "Valid property",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_VALID, param_spec);

  param_spec = g_param_spec_boolean ("enabled", "enabled",
      "Enabled property",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ENABLED, param_spec);

  param_spec = g_param_spec_string ("nickname", "nickname",
      "Nickname property",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NICKNAME, param_spec);

  param_spec = g_param_spec_boxed ("parameters", "parameters",
      "Parameters property",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PARAMETERS, param_spec);

  param_spec = g_param_spec_boxed ("automatic-presence", "automatic presence",
      "AutomaticPresence property",
      TP_STRUCT_TYPE_SIMPLE_PRESENCE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_AUTOMATIC_PRESENCE,
      param_spec);

  param_spec = g_param_spec_boolean ("connect-automatically",
      "connect automatically", "ConnectAutomatically property",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECT_AUTO, param_spec);

  param_spec = g_param_spec_boxed ("connection", "connection",
      "Connection property",
      DBUS_TYPE_G_OBJECT_PATH,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

  param_spec = g_param_spec_uint ("connection-status", "connection status",
      "ConnectionStatus property",
      0, NUM_TP_CONNECTION_STATUSES, TP_CONNECTION_STATUS_DISCONNECTED,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION_STATUS,
      param_spec);

  param_spec = g_param_spec_uint ("connection-status-reason",
      "connection status reason", "ConnectionStatusReason property",
      0, NUM_TP_CONNECTION_STATUS_REASONS,
      TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION_STATUS_REASON,
      param_spec);

  param_spec = g_param_spec_boxed ("current-presence", "current presence",
      "CurrentPresence property",
      TP_STRUCT_TYPE_SIMPLE_PRESENCE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CURRENT_PRESENCE,
      param_spec);

  param_spec = g_param_spec_boxed ("requested-presence", "requested presence",
      "RequestedPresence property",
      TP_STRUCT_TYPE_SIMPLE_PRESENCE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REQUESTED_PRESENCE,
      param_spec);

  param_spec = g_param_spec_string ("normalized-name", "normalized name",
      "NormalizedName property",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NORMALIZED_NAME,
      param_spec);

  param_spec = g_param_spec_boolean ("has-been-online", "has been online",
      "HasBeenOnline property",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_HAS_BEEN_ONLINE,
      param_spec);

  param_spec = g_param_spec_string ("storage-provider", "storage provider",
      "StorageProvider property",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STORAGE_PROVIDER,
      param_spec);

  param_spec = g_param_spec_boxed ("storage-identifier", "storage identifier",
      "StorageIdentifier property",
      G_TYPE_VALUE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STORAGE_IDENTIFIER,
      param_spec);

  param_spec = g_param_spec_boxed ("storage-specific-information",
      "storage specific information", "StorageSpecificInformation property",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_STORAGE_SPECIFIC_INFORMATION, param_spec);

  param_spec = g_param_spec_uint ("storage-restrictions",
      "storage restrictions", "StorageRestrictions property",
      0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STORAGE_RESTRICTIONS,
      param_spec);

  klass->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpTestsSimpleAccountClass, dbus_props_class));
}
