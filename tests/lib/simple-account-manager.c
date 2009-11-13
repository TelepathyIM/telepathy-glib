/*
 * simple-account-manager.c - a simple account manager service.
 *
 * Copyright (C) 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "simple-account-manager.h"

#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-generic.h>

G_DEFINE_TYPE_WITH_CODE (SimpleAccountManager,
    simple_account_manager,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
        tp_dbus_properties_mixin_iface_init)
    )

/* type definition stuff */

/* TP_IFACE_ACCOUNT_MANAGER is implied */
static const char *ACCOUNT_MANAGER_INTERFACES[] = { NULL };

enum
{
  PROP_0,
  PROP_INTERFACES,
};

struct _SimpleAccountManagerPrivate
{
  int dummy;
};

static void
simple_account_manager_init (SimpleAccountManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, SIMPLE_TYPE_ACCOUNT_MANAGER,
      SimpleAccountManagerPrivate);
}

static void
simple_account_manager_get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *spec)
{
  switch (property_id) {
    case PROP_INTERFACES:
      g_value_set_boxed (value, ACCOUNT_MANAGER_INTERFACES);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
      break;
  }
}

/**
  * This class currently only provides the minimum for
  * tp_account_manager_prepare to succeed. This turns out to be only a working
  * Properties.GetAll(). If we wanted later to check the case where
  * tp_account_prepare succeeds, we would need to implement an account object
  * too. In that case, it might be worth using TpSvcAccountManager
  * as well as/instead of TpDBusPropertiesMixinPropImpl.
  */
static void
simple_account_manager_class_init (SimpleAccountManagerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  static TpDBusPropertiesMixinPropImpl am_props[] = {
        { "Interfaces", "interfaces", NULL },
        /*
        { "ValidAccounts", "interfaces", NULL },
        { "InvalidAccounts", "invalid-accounts", NULL },
        { "SupportedAccountProperties", "supported-account-properties", NULL },
        */
        { NULL }
  };

  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
        { NULL },
        { TP_IFACE_ACCOUNT_MANAGER,
          tp_dbus_properties_mixin_getter_gobject_properties,
          NULL,
          am_props
        },
        { NULL },
  };

  g_type_class_add_private (klass, sizeof (SimpleAccountManagerPrivate));
  object_class->get_property = simple_account_manager_get_property;

  param_spec = g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
      "In this case we only implement AccountManager, so none.",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);
  klass->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (SimpleAccountManagerClass, dbus_props_class));
}
