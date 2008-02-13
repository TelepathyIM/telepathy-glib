/*
 * dbus-properties-mixin.c - D-Bus core Properties
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008 Nokia Corporation
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

/**
 * SECTION:dbus-properties-mixin
 * @title: TpDBusPropertiesMixin
 * @short_description: a mixin implementation of the DBus.Properties interface
 * @see_also: #TpSvcDBusProperties
 */

#include <telepathy-glib/dbus-properties-mixin.h>

#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/util.h>

static GQuark
_info_quark (void)
{
  static GQuark q = 0;

  if (G_UNLIKELY (q == 0))
    q = g_quark_from_static_string
        ("tp_svc_interface_get_dbus_properties_info@TELEPATHY_GLIB_0.7.3");

  return q;
}

void
tp_svc_interface_set_dbus_properties_info (GType g_interface,
    TpDBusPropertiesMixinIfaceInfo *info)
{
  GQuark q = _info_quark ();
  TpDBusPropertiesMixinPropInfo *prop;

  g_return_if_fail (G_TYPE_IS_INTERFACE (g_interface));
  g_return_if_fail (g_type_get_qdata (g_interface, q) == NULL);
  g_return_if_fail (info->dbus_interface != 0);
  g_return_if_fail (info->props != NULL);

  for (prop = info->props; prop->name != 0; prop++)
    {
      g_return_if_fail (prop->flags != 0);
      g_return_if_fail (prop->flags <= (TP_DBUS_PROPERTIES_MIXIN_FLAG_READ |
            TP_DBUS_PROPERTIES_MIXIN_FLAG_WRITE));
      g_return_if_fail (prop->dbus_signature != NULL);
      g_return_if_fail (prop->dbus_signature[0] != '\0');
      g_return_if_fail (prop->type != 0);
    }

  g_type_set_qdata (g_interface, q, info);
}

TpDBusPropertiesMixinIfaceInfo *
tp_svc_interface_get_dbus_properties_info (GType g_interface)
{
  return g_type_get_qdata (g_interface, _info_quark ());
}

static GQuark
_mixin_quark (void)
{
  static GQuark q = 0;

  if (G_UNLIKELY (q == 0))
    q = g_quark_from_static_string
        ("tp_dbus_properties_mixin_class_init@TELEPATHY_GLIB_0.7.3");

  return q;
}

/**
 * tp_dbus_properties_mixin_class_init:
 * @cls: a subclass of #GObjectClass
 * @offset: the offset within @cls of a TpDBusPropertiesMixinClass structure
 *
 * Initialize the class @cls to use the D-Bus Properties mixin.
 * The given struct member, of size sizeof(TpDBusPropertiesMixinClass),
 * will be used to store property implementation information.
 *
 * Before calling this function, the array of interfaces must have been
 * placed in the TpDBusPropertiesMixinClass structure.
 */
void
tp_dbus_properties_mixin_class_init (GObjectClass *cls,
                                     gsize offset)
{
  GQuark q = _mixin_quark ();
  GType type = G_OBJECT_CLASS_TYPE (cls);
  TpDBusPropertiesMixinClass *mixin;
  TpDBusPropertiesMixinIfaceImpl *iface_impl;

  g_return_if_fail (G_IS_OBJECT_CLASS (cls));
  g_return_if_fail (g_type_get_qdata (type, q) == NULL);
  g_type_set_qdata (type, q, GSIZE_TO_POINTER (offset));

  mixin = &G_STRUCT_MEMBER (TpDBusPropertiesMixinClass, cls, offset);

  g_return_if_fail (mixin->interfaces != NULL);

  for (iface_impl = mixin->interfaces;
       iface_impl->dbus_interface != 0;
       iface_impl++)
    {
      TpDBusPropertiesMixinIfaceInfo *iface_info;
      TpDBusPropertiesMixinPropImpl *prop_impl;

      g_return_if_fail (G_TYPE_IS_INTERFACE (iface_impl->svc_interface));
      g_return_if_fail (iface_impl->props != NULL);

      iface_info = tp_svc_interface_get_dbus_properties_info
          (iface_impl->svc_interface);

      g_return_if_fail (iface_info != NULL);
      g_return_if_fail (iface_impl->dbus_interface ==
          iface_info->dbus_interface);

      for (prop_impl = iface_impl->props; prop_impl->name != NULL; prop_impl++)
        {
          TpDBusPropertiesMixinPropInfo *prop_info;
          GQuark name_quark = g_quark_try_string (prop_impl->name);

          prop_impl->mixin_priv = NULL;

          /* no point bothering if there is no quark for this name */
          if (name_quark != 0)
            {
              for (prop_info = iface_info->props;
                   prop_info->name != 0;
                   prop_info++)
                {
                  if (prop_info->name == name_quark)
                    prop_impl->mixin_priv = prop_info;
                }
            }

          if (prop_impl->mixin_priv == NULL)
            {
              g_critical ("%s tried to implement nonexistent property %s"
                  "on interface %s", g_type_name (type), prop_impl->name,
                  g_quark_to_string (iface_impl->dbus_interface));
              return;
            }
        }
    }
}

static TpDBusPropertiesMixinIfaceImpl *
_tp_dbus_properties_mixin_find_iface_impl (GObject *self,
                                           const gchar *name,
                                           DBusGMethodInvocation *context)
{
  GType type;
  GQuark q = _mixin_quark ();
  GQuark iface_quark = g_quark_try_string (name);

  if (iface_quark == 0)
    return NULL;

  for (type = G_OBJECT_TYPE (self);
       type != 0;
       type = g_type_parent (type))
    {
      gpointer offset = g_type_get_qdata (type, q);
      TpDBusPropertiesMixinClass *mixin;
      TpDBusPropertiesMixinIfaceImpl *iface_impl;

      if (offset == NULL)
        continue;

      mixin = &G_STRUCT_MEMBER (TpDBusPropertiesMixinClass,
          G_OBJECT_GET_CLASS (self), GPOINTER_TO_SIZE (offset));

      for (iface_impl = mixin->interfaces;
           iface_impl->dbus_interface != 0;
           iface_impl++)
        {
          if (iface_impl->dbus_interface == iface_quark)
            return iface_impl;
        }
    }

  return NULL;
}

static TpDBusPropertiesMixinPropImpl *
_tp_dbus_properties_mixin_find_prop_impl
    (TpDBusPropertiesMixinIfaceImpl *iface_impl,
     const gchar *name,
     DBusGMethodInvocation *context)
{
  GQuark prop_quark = g_quark_try_string (name);
  TpDBusPropertiesMixinPropImpl *prop_impl;

  if (prop_quark == 0)
    return NULL;

  for (prop_impl = iface_impl->props;
       prop_impl->name != NULL;
       prop_impl++)
    {
      TpDBusPropertiesMixinPropInfo *prop_info = prop_impl->mixin_priv;

      if (prop_info->name == prop_quark)
        return prop_impl;
    }

  return NULL;
}

static void
_tp_dbus_properties_mixin_get (TpSvcDBusProperties *iface,
                               const gchar *interface_name,
                               const gchar *property_name,
                               DBusGMethodInvocation *context)
{
  GObject *self = G_OBJECT (iface);
  TpDBusPropertiesMixinIfaceImpl *iface_impl;
  TpDBusPropertiesMixinPropImpl *prop_impl;
  TpDBusPropertiesMixinPropInfo *prop_info;
  GValue value = { 0 };

  iface_impl = _tp_dbus_properties_mixin_find_iface_impl (self,
      interface_name, context);

  if (iface_impl == NULL)
    return;

  prop_impl = _tp_dbus_properties_mixin_find_prop_impl (iface_impl,
      property_name, context);

  if (prop_impl == NULL)
    return;

  prop_info = prop_impl->mixin_priv;

  if ((prop_info->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_READ) == 0)
    {
      GError e = { DBUS_GERROR, DBUS_GERROR_NOT_SUPPORTED,
          "This property is write-only" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  g_value_init (&value, prop_info->type);
  iface_impl->getter (self, iface_impl->dbus_interface,
      prop_info->name, &value, prop_impl->data);
  tp_svc_dbus_properties_return_from_get (context, &value);
  g_value_unset (&value);
}

static void
_tp_dbus_properties_mixin_get_all (TpSvcDBusProperties *iface,
                                   const gchar *interface_name,
                                   DBusGMethodInvocation *context)
{
  GObject *self = G_OBJECT (iface);
  TpDBusPropertiesMixinIfaceImpl *iface_impl;
  TpDBusPropertiesMixinPropImpl *prop_impl;
  /* no key destructor needed - the keys are immortal */
  GHashTable *values = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

  iface_impl = _tp_dbus_properties_mixin_find_iface_impl (self,
      interface_name, context);

  if (iface_impl == NULL)
    return;

  for (prop_impl = iface_impl->props;
       prop_impl->name != NULL;
       prop_impl++)
    {
      TpDBusPropertiesMixinPropInfo *prop_info = prop_impl->mixin_priv;
      GValue *value;

      if ((prop_info->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_READ) == 0)
        continue;

      value = tp_g_value_slice_new (prop_info->type);
      iface_impl->getter (self, iface_impl->dbus_interface,
          prop_info->name, value, prop_impl->data);
      g_hash_table_insert (values, (gchar *) prop_impl->name, value);
    }

  tp_svc_dbus_properties_return_from_get_all (context, values);
  g_hash_table_destroy (values);
}

static void
_tp_dbus_properties_mixin_set (TpSvcDBusProperties *iface,
                               const gchar *interface_name,
                               const gchar *property_name,
                               const GValue *value,
                               DBusGMethodInvocation *context)
{
  GObject *self = G_OBJECT (iface);
  TpDBusPropertiesMixinIfaceImpl *iface_impl;
  TpDBusPropertiesMixinPropImpl *prop_impl;
  TpDBusPropertiesMixinPropInfo *prop_info;
  GValue copy = { 0 };
  GError *error = NULL;

  iface_impl = _tp_dbus_properties_mixin_find_iface_impl (self,
      interface_name, context);

  if (iface_impl == NULL)
    return;

  prop_impl = _tp_dbus_properties_mixin_find_prop_impl (iface_impl,
      property_name, context);

  if (prop_impl == NULL)
    return;

  prop_info = prop_impl->mixin_priv;

  if ((prop_info->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_WRITE) == 0)
    {
      GError e = { DBUS_GERROR, DBUS_GERROR_NOT_SUPPORTED,
          "This property is read-only" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  if (G_VALUE_TYPE (value) != prop_info->type)
    {
      g_value_init (&copy, prop_info->type);

      if (!g_value_transform (value, &copy))
        {
          error = g_error_new (DBUS_GERROR, DBUS_GERROR_NOT_SUPPORTED,
              "Cannot convert %s to %s for property %s",
              g_type_name (G_VALUE_TYPE (value)),
              g_type_name (prop_info->type),
              property_name);

          dbus_g_method_return_error (context, error);
          g_error_free (error);
        }
    }

  if (iface_impl->setter (self, iface_impl->dbus_interface,
        prop_info->name, value, prop_impl->data, &error))
    {
      tp_svc_dbus_properties_return_from_get (context, value);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }

  if (G_IS_VALUE (&copy))
    g_value_unset (&copy);
}

void
tp_dbus_properties_mixin_iface_init (gpointer g_iface,
                                     gpointer iface_data)
{
  TpSvcDBusPropertiesClass *cls = g_iface;

#define IMPLEMENT(x) \
    tp_svc_dbus_properties_implement_##x (cls, _tp_dbus_properties_mixin_##x)
  IMPLEMENT (get);
  IMPLEMENT (get_all);
  IMPLEMENT (set);
#undef IMPLEMENT
}
