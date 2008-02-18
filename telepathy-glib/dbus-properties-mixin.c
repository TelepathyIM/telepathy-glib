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

#include <telepathy-glib/dbus-properties-mixin.h>

#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/util.h>

/**
 * SECTION:dbus-properties-mixin
 * @title: TpDBusPropertiesMixin
 * @short_description: a mixin implementation of the DBus.Properties interface
 * @see_also: #TpSvcDBusProperties
 *
 * This mixin provides an implementation of the org.freedesktop.DBus.Properties
 * interface. It relies on the auto-generated service-side GInterfaces from
 * telepathy-glib >= 0.7.3, or something similar, to register the abstract
 * properties and their GTypes; classes with the mixin can then register
 * an implementation of the properties.
 *
 * To register D-Bus properties in a GInterface to be implementable with this
 * mixin, either use the code-generation tools from telepathy-glib >= 0.7.3,
 * or call tp_svc_interface_set_properties_info() from a section of the
 * base_init function that only runs once.
 *
 * To use this mixin, include a #TpDBusPropertiesMixinClass somewhere
 * in your class structure, populate it with pointers to statically allocated
 * (or duplicated and never freed) data, and call
 * tp_dbus_properties_mixin_class_init() from your class_init implementation.
 *
 * To use this mixin as the implementation of #TpSvcDBusProperties,
 * call <literal>G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
 * tp_dbus_properties_mixin_iface_init)</literal> in the fourth argument to
 * <literal>G_DEFINE_TYPE_WITH_CODE</literal>.
 *
 * Since: 0.7.3
 */

/**
 * TpDBusPropertiesMixinFlags:
 * @TP_DBUS_PROPERTIES_MIXIN_FLAG_READ: The property can be read using Get and
 *  GetAll
 * @TP_DBUS_PROPERTIES_MIXIN_FLAG_WRITE: The property can be written using Set
 *
 * Bitfield representing allowed access to a property.
 *
 * Since: 0.7.3
 */

/**
 * TpDBusPropertiesMixinPropInfo:
 * @name: Quark representing the property's name
 * @flags: Flags representing read/write access to the property
 * @dbus_signature: The D-Bus signature of the property
 * @type: The GType used in a GValue to implement the property
 *
 * Semi-abstract description of a property, as attached to a service
 * GInterface. This structure must either be statically allocated, or
 * duplicated and never freed, so it always remains valid.
 *
 * In addition to the documented members, there are two private pointers
 * for future expansion, which must always be initialized to %NULL.
 *
 * Since: 0.7.3
 */

/**
 * TpDBusPropertiesMixinIfaceInfo:
 * @dbus_interface: Quark representing the interface's name
 * @props: Array of property descriptions, terminated by one with
 *  @name == %NULL
 *
 * Semi-abstract description of an interface. Each service GInterface that
 * has properties must have one of these attached to it via
 * tp_svc_interface_set_dbus_properties_info() in its base_init function;
 * service GInterfaces that do not have properties may have one of these
 * with no properties.
 *
 * This structure must either be statically allocated, or duplicated and never
 * freed, so it always remains valid.
 *
 * In addition to the documented members, there are two private pointers
 * for future expansion, which must always be initialized to %NULL.
 *
 * Since: 0.7.3
 */

static GQuark
_info_quark (void)
{
  static GQuark q = 0;

  if (G_UNLIKELY (q == 0))
    q = g_quark_from_static_string
        ("tp_svc_interface_get_dbus_properties_info@TELEPATHY_GLIB_0.7.3");

  return q;
}

/**
 * tp_svc_interface_set_dbus_properties_info:
 * @g_interface: The #GType of a service interface
 * @info: an interface description
 *
 * Declare that @g_interface implements the given D-Bus interface, with the
 * given properties. This may only be called once per GInterface, usually from
 * a section of its base_init function that only runs once.
 *
 * Since: 0.7.3
 */
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

/* could make this public, but it doesn't seem necessary yet */
static TpDBusPropertiesMixinIfaceInfo *
tp_svc_interface_get_dbus_properties_info (GType g_interface)
{
  return g_type_get_qdata (g_interface, _info_quark ());
}

/**
 * TpDBusPropertiesMixinGetter:
 * @object: The exported object with the properties
 * @interface: A quark representing the D-Bus interface name
 * @name: A quark representing the D-Bus property name
 * @value: A GValue pre-initialized to the right type, into which to put
 *  the value
 * @getter_data: The getter_data from the #TpDBusPropertiesMixinPropImpl
 *
 * Signature of a callback used to get the value of a property.
 *
 * For simplicity, in this mixin we don't allow getting a property to fail;
 * implementations must always be prepared to return *something*.
 */

/**
 * tp_dbus_properties_mixin_getter_gobject_properties:
 * @object: The exported object with the properties
 * @interface: A quark representing the D-Bus interface name
 * @name: A quark representing the D-Bus property name
 * @value: A GValue pre-initialized to the right type, into which to put
 *  the value
 * @getter_data: The getter_data from the #TpDBusPropertiesMixinPropImpl,
 *  which must be a string containing the GObject property's name
 *
 * An implementation of #TpDBusPropertiesMixinGetter which assumes that
 * the @getter_data is the name of a readable #GObject property of an
 * appropriate type, and uses it for the value of the D-Bus property.
 */
void
tp_dbus_properties_mixin_getter_gobject_properties (GObject *object,
                                                    GQuark interface,
                                                    GQuark name,
                                                    GValue *value,
                                                    gpointer getter_data)
{
  g_object_get_property (object, getter_data, value);
}

/**
 * TpDBusPropertiesMixinSetter:
 * @object: The exported object with the properties
 * @interface: A quark representing the D-Bus interface name
 * @name: A quark representing the D-Bus property name
 * @value: The new value for the property
 * @setter_data: The setter_data from the #TpDBusPropertiesMixinPropImpl
 * @error: Used to return an error on failure
 *
 * Signature of a callback used to get the value of a property.
 *
 * Returns: %TRUE on success, %FALSE (setting @error) on failure
 */

/**
 * tp_dbus_properties_mixin_setter_gobject_properties:
 * @object: The exported object with the properties
 * @interface: A quark representing the D-Bus interface name
 * @name: A quark representing the D-Bus property name
 * @value: The new value for the property
 * @setter_data: The setter_data from the #TpDBusPropertiesMixinPropImpl,
 *  which must be a string containing the GObject property's name
 * @error: Not used
 *
 * An implementation of #TpDBusPropertiesMixinSetter which assumes that the
 * @setter_data is the name of a writable #GObject property of an appropriate
 * type, and sets that property to the given value.
 *
 * Returns: %TRUE
 */
gboolean
tp_dbus_properties_mixin_setter_gobject_properties (GObject *object,
                                                    GQuark interface,
                                                    GQuark name,
                                                    const GValue *value,
                                                    gpointer setter_data,
                                                    GError **error)
{
  g_object_set_property (object, setter_data, value);
  return TRUE;
}

/**
 * TpDBusPropertiesMixinPropImpl:
 * @name: The name of the property as it appears on D-Bus
 * @getter_data: Arbitrary user-supplied data for the getter function
 * @setter_data: Arbitrary user-supplied data for the setter function
 *
 * Structure representing an implementation of a property.
 *
 * In addition to the documented fields, there are three pointers which must
 * be initialized to %NULL.
 *
 * This structure must either be statically allocated, or duplicated and never
 * freed, so it always remains valid.
 *
 * Since: 0.7.3
 */

/**
 * TpDBusPropertiesMixinIfaceImpl:
 * @name: The name of the interface
 * @getter: A callback to get the current value of the property, to which
 *  the @getter_data from each property implementation will be passed
 * @setter: A callback to set a new value for the property, to which
 *  the @setter_data from each property implementation will be passed
 * @props: An array of property implementations, terminated by one with
 *  @name equal to %NULL
 *
 * Structure representing an implementation of an interface's properties.
 *
 * In addition to the documented fields, there are three pointers which must
 * be initialized to %NULL.
 *
 * This structure must either be statically allocated, or duplicated and never
 * freed, so it always remains valid.
 *
 * Since: 0.7.3
 */

/**
 * TpDBusPropertiesMixinClass:
 * @interfaces: An array of interface implementations, terminated by one with
 *  @name equal to %NULL
 *
 * Structure representing all of a class's property implementations. One of
 * these structures may be placed in the layout of an object class structure.
 *
 * In addition to the documented fields, there are 7 pointers reserved for
 * future use, which must be initialized to %NULL.
 *
 * Since: 0.7.3
 */

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
 * Each property and each interface must have been declared as a member of
 * a GInterface implemented by @cls, using
 * tp_svc_interface_set_dbus_properties_info().
 *
 * Before calling this function, the array of interfaces must have been
 * placed in the TpDBusPropertiesMixinClass structure.
 *
 * Since: 0.7.3
 */
void
tp_dbus_properties_mixin_class_init (GObjectClass *cls,
                                     gsize offset)
{
  GQuark q = _mixin_quark ();
  GType type = G_OBJECT_CLASS_TYPE (cls);
  TpDBusPropertiesMixinClass *mixin;
  TpDBusPropertiesMixinIfaceImpl *iface_impl;
  GType *interfaces, *iface;

  g_return_if_fail (G_IS_OBJECT_CLASS (cls));
  g_return_if_fail (g_type_get_qdata (type, q) == NULL);
  g_type_set_qdata (type, q, GSIZE_TO_POINTER (offset));

  mixin = &G_STRUCT_MEMBER (TpDBusPropertiesMixinClass, cls, offset);

  g_return_if_fail (mixin->interfaces != NULL);

  interfaces = g_type_interfaces (type, NULL);

  for (iface_impl = mixin->interfaces;
       iface_impl->name != NULL;
       iface_impl++)
    {
      TpDBusPropertiesMixinIfaceInfo *iface_info = NULL;
      TpDBusPropertiesMixinPropImpl *prop_impl;
      GQuark iface_quark = g_quark_try_string (iface_impl->name);

      g_return_if_fail (iface_impl->props != NULL);

      /* no point bothering if there is no quark for the interface name */
      if (iface_quark != 0)
        {
          for (iface = interfaces; *iface != 0; iface++)
            {
              iface_info = tp_svc_interface_get_dbus_properties_info (*iface);

              if (iface_info != NULL &&
                  iface_info->dbus_interface == iface_quark)
                break;
              else
                iface_info = NULL;
            }
        }

      if (iface_info == NULL)
        {
          g_critical ("%s tried to implement undefined interface %s",
              g_type_name (type), iface_impl->name);
          goto out;
        }

      iface_impl->mixin_priv = iface_info;

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
                    {
                      prop_impl->mixin_priv = prop_info;
                      break;
                    }
                }
            }

          if (prop_impl->mixin_priv == NULL)
            {
              g_critical ("%s tried to implement nonexistent property %s"
                  "on interface %s", g_type_name (type), prop_impl->name,
                  iface_impl->name);
              goto out;
            }
        }
    }

out:
  g_free (interfaces);
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
      TpDBusPropertiesMixinIfaceInfo *iface_info;

      if (offset == NULL)
        continue;

      mixin = &G_STRUCT_MEMBER (TpDBusPropertiesMixinClass,
          G_OBJECT_GET_CLASS (self), GPOINTER_TO_SIZE (offset));

      for (iface_impl = mixin->interfaces;
           iface_impl->name != NULL;
           iface_impl++)
        {
          iface_info = iface_impl->mixin_priv;

          if (iface_info->dbus_interface == iface_quark)
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
  TpDBusPropertiesMixinIfaceInfo *iface_info;
  TpDBusPropertiesMixinPropImpl *prop_impl;
  TpDBusPropertiesMixinPropInfo *prop_info;
  GValue value = { 0 };

  iface_impl = _tp_dbus_properties_mixin_find_iface_impl (self,
      interface_name, context);

  if (iface_impl == NULL)
    return;

  iface_info = iface_impl->mixin_priv;

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
  iface_impl->getter (self, iface_info->dbus_interface,
      prop_info->name, &value, prop_impl->getter_data);
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
  TpDBusPropertiesMixinIfaceInfo *iface_info;
  TpDBusPropertiesMixinPropImpl *prop_impl;
  /* no key destructor needed - the keys are immortal */
  GHashTable *values = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

  iface_impl = _tp_dbus_properties_mixin_find_iface_impl (self,
      interface_name, context);

  if (iface_impl == NULL)
    return;

  iface_info = iface_impl->mixin_priv;

  for (prop_impl = iface_impl->props;
       prop_impl->name != NULL;
       prop_impl++)
    {
      TpDBusPropertiesMixinPropInfo *prop_info = prop_impl->mixin_priv;
      GValue *value;

      if ((prop_info->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_READ) == 0)
        continue;

      value = tp_g_value_slice_new (prop_info->type);
      iface_impl->getter (self, iface_info->dbus_interface,
          prop_info->name, value, prop_impl->getter_data);
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
  TpDBusPropertiesMixinIfaceInfo *iface_info;
  TpDBusPropertiesMixinPropImpl *prop_impl;
  TpDBusPropertiesMixinPropInfo *prop_info;
  GValue copy = { 0 };
  GError *error = NULL;

  iface_impl = _tp_dbus_properties_mixin_find_iface_impl (self,
      interface_name, context);

  if (iface_impl == NULL)
    return;

  iface_info = iface_impl->mixin_priv;

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

  if (iface_impl->setter (self, iface_info->dbus_interface,
        prop_info->name, value, prop_impl->setter_data, &error))
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

/**
 * tp_dbus_properties_mixin_iface_init:
 * @g_iface: a pointer to a #TpSvcDBusPropertiesClass structure
 * @iface_data: ignored
 *
 * Declare that the DBus.Properties interface represented by @g_iface
 * is implemented using this mixin.
 */
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
