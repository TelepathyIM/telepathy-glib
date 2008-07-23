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

#include "config.h"

#include <telepathy-glib/dbus-properties-mixin.h>

#include <telepathy-glib/errors.h>
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
_iface_prop_info_quark (void)
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
  GQuark q = _iface_prop_info_quark ();
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
  return g_type_get_qdata (g_interface, _iface_prop_info_quark ());
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
 * In addition to the documented fields, there are four pointers which must
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
_prop_mixin_offset_quark (void)
{
  static GQuark q = 0;

  if (G_UNLIKELY (q == 0))
    q = g_quark_from_static_string
        ("tp_dbus_properties_mixin_class_init@TELEPATHY_GLIB_0.7.3");

  return q;
}

static GQuark
_extra_prop_impls_quark (void)
{
  static GQuark q = 0;

  if (G_UNLIKELY (q == 0))
    q = g_quark_from_static_string
        ("tp_dbus_properties_mixin_implement_interface@TELEPATHY_GLIB_0.7.9");

  return q;
}


static gboolean
link_interface (GType type,
                const GType *interfaces,
                GQuark iface_quark,
                TpDBusPropertiesMixinIfaceImpl *iface_impl)
{
  TpDBusPropertiesMixinIfaceInfo *iface_info = NULL;
  TpDBusPropertiesMixinPropImpl *prop_impl;

  g_return_val_if_fail (iface_impl->props != NULL, FALSE);

  /* no point bothering if there is no quark for the interface name */
  if (iface_quark != 0)
    {
      const GType *iface;

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
      return FALSE;
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
          return FALSE;
        }
    }

  return TRUE;
}


/**
 * tp_dbus_properties_mixin_implement_interface:
 * @cls: a subclass of #GObjectClass
 * @iface: a quark representing the the name of the interface to implement
 * @getter: a callback to get properties on this interface, or %NULL if they
 *  are all write-only
 * @setter: a callback to set properties on this interface, or %NULL if they
 *  are all read-only
 * @props: an array of #TpDBusPropertiesMixinPropImpl representing individual
 *  properties, terminated by one with @name == %NULL
 *
 * Declare that, in addition to any interfaces set in
 * tp_dbus_properties_mixin_class_init(), the given class (and its subclasses)
 * will implement the properties of the interface @iface using the callbacks
 * @getter and @setter and the properties given by @props.
 *
 * This function should be called from the class_init callback in such a way
 * that it will only be called once, even if the class is subclassed.
 *
 * Typically, the static array @interfaces in the #TpDBusPropertiesMixinClass
 * should be used for interfaces whose properties are implemented directly by
 * the class @cls, and this function should be used for interfaces whose
 * properties are implemented by mixins.
 *
 * It is an error for the same interface to appear in the array @interfaces
 * in the #TpDBusPropertiesMixinClass, and also be set up by this function.
 *
 * If a class C and a subclass S both implement the properties of the same
 * interface, only the implementations from the subclass S will be used,
 * regardless of whether the implementations in C and/or S were set up by
 * this function or via the array @interfaces in the
 * #TpDBusPropertiesMixinClass.
 */
void
tp_dbus_properties_mixin_implement_interface (GObjectClass *cls,
    GQuark iface,
    TpDBusPropertiesMixinGetter getter,
    TpDBusPropertiesMixinSetter setter,
    TpDBusPropertiesMixinPropImpl *props)
{
  GQuark offset_quark = _prop_mixin_offset_quark ();
  GQuark extras_quark = _extra_prop_impls_quark ();
  GType type = G_OBJECT_CLASS_TYPE (cls);
  GType *interfaces = g_type_interfaces (type, NULL);
  TpDBusPropertiesMixinClass *mixin;
  gpointer offset_qdata;
  TpDBusPropertiesMixinIfaceImpl *iface_impl;

  g_return_if_fail (G_IS_OBJECT_CLASS (cls));

  offset_qdata = g_type_get_qdata (type, offset_quark);

  g_return_if_fail (offset_qdata != NULL);

  mixin = &G_STRUCT_MEMBER (TpDBusPropertiesMixinClass, cls,
      GPOINTER_TO_SIZE (offset_qdata));

  /* never freed - intentional per-class leak */
  iface_impl = g_new0 (TpDBusPropertiesMixinIfaceImpl, 1);
  iface_impl->name = g_quark_to_string (iface);
  iface_impl->getter = getter;
  iface_impl->setter = setter;
  iface_impl->props = props;

  /* align property implementations with abstract properties */
  if (G_LIKELY (link_interface (type, interfaces, iface, iface_impl)))
    {
      TpDBusPropertiesMixinIfaceImpl *next = g_type_get_qdata (type,
          extras_quark);
#ifdef ENABLE_DEBUG
      TpDBusPropertiesMixinIfaceImpl *iter;

      /* assert that we're not trying to implement the same interface twice */
      for (iter = next;
           iter != NULL && iter->name != NULL;
           iter = iter->mixin_next.priv)
        {
          TpDBusPropertiesMixinIfaceInfo *other_info = iter->mixin_priv;

          g_assert (other_info != NULL);

          if (G_UNLIKELY (other_info->dbus_interface == iface))
            {
              g_critical ("type %s tried to implement interface %s with %s "
                  "twice", g_type_name (type), g_quark_to_string (iface),
                  G_STRFUNC);
              goto out;
            }
        }

      /* assert that we're not trying to implement the same interface via
       * this function and the static data */
      for (iter = mixin->interfaces;
           iter != NULL && iter->name != NULL;
           iter++)
        {
          TpDBusPropertiesMixinIfaceInfo *other_info = iter->mixin_priv;

          g_assert (other_info != NULL);

          if (G_UNLIKELY (other_info->dbus_interface == iface))
            {
              g_critical ("type %s tried to implement interface %s with %s "
                  "and also in static data", g_type_name (type),
                  g_quark_to_string (iface), G_STRFUNC);
              goto out;
            }
        }
#endif

      /* form a linked list */
      iface_impl->mixin_next.priv = next;
      g_type_set_qdata (type, extras_quark, iface_impl);
    }

#ifdef ENABLE_DEBUG
out:
#endif
  g_free (interfaces);
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
 * Before calling this function, the array @interfaces must have been
 * placed in the #TpDBusPropertiesMixinClass structure; if it would be empty,
 * it may instead be %NULL.
 *
 * This function should be called from the class_init callback in such a way
 * that it will only be called once, even if the class is subclassed.
 *
 * Changed in 0.7.9: TpDBusPropertiesMixinClass::interfaces may now be %NULL,
 * which means that only interfaces whose properties are set up using
 * tp_dbus_properties_mixin_implement_interface() will be used.
 *
 * Since: 0.7.3
 */
void
tp_dbus_properties_mixin_class_init (GObjectClass *cls,
                                     gsize offset)
{
  GQuark q = _prop_mixin_offset_quark ();
  GType type = G_OBJECT_CLASS_TYPE (cls);
  TpDBusPropertiesMixinClass *mixin;
  TpDBusPropertiesMixinIfaceImpl *iface_impl;
  GType *interfaces;

  g_return_if_fail (G_IS_OBJECT_CLASS (cls));
  g_return_if_fail (g_type_get_qdata (type, q) == NULL);
  g_type_set_qdata (type, q, GSIZE_TO_POINTER (offset));

  mixin = &G_STRUCT_MEMBER (TpDBusPropertiesMixinClass, cls, offset);

  interfaces = g_type_interfaces (type, NULL);

  for (iface_impl = mixin->interfaces;
       iface_impl != NULL && iface_impl->name != NULL;
       iface_impl++)
    {
      GQuark iface_quark = g_quark_try_string (iface_impl->name);
#ifdef ENABLE_DEBUG
      TpDBusPropertiesMixinIfaceImpl *other_impl;
#endif

      if (G_UNLIKELY (!link_interface (type, interfaces, iface_quark,
              iface_impl)))
        goto out;

#ifdef ENABLE_DEBUG
      for (other_impl = mixin->interfaces;
           other_impl != iface_impl;
           other_impl++)
        {
          TpDBusPropertiesMixinIfaceInfo *other_info = other_impl->mixin_priv;

          if (G_UNLIKELY (iface_quark == other_info->dbus_interface))
            {
              g_critical ("type %s tried to implement interface %s in static "
                  "data twice", g_type_name (type), iface_impl->name);
              goto out;
            }
        }
#endif
    }

out:

  g_free (interfaces);
}

static TpDBusPropertiesMixinIfaceImpl *
_tp_dbus_properties_mixin_find_iface_impl (GObject *self,
                                           const gchar *name)
{
  GType type;
  GQuark offset_quark = _prop_mixin_offset_quark ();
  GQuark extras_quark = _extra_prop_impls_quark ();
  GQuark iface_quark = g_quark_try_string (name);

  if (iface_quark == 0)
    return NULL;

  for (type = G_OBJECT_TYPE (self);
       type != 0;
       type = g_type_parent (type))
    {
      gpointer offset = g_type_get_qdata (type, offset_quark);
      TpDBusPropertiesMixinClass *mixin;
      TpDBusPropertiesMixinIfaceImpl *iface_impl;
      TpDBusPropertiesMixinIfaceInfo *iface_info;

      if (offset == NULL)
        continue;

      mixin = &G_STRUCT_MEMBER (TpDBusPropertiesMixinClass,
          G_OBJECT_GET_CLASS (self), GPOINTER_TO_SIZE (offset));

      for (iface_impl = mixin->interfaces;
           iface_impl != NULL && iface_impl->name != NULL;
           iface_impl++)
        {
          iface_info = iface_impl->mixin_priv;

          if (iface_info->dbus_interface == iface_quark)
            return iface_impl;
        }

      for (iface_impl = g_type_get_qdata (type, extras_quark);
           iface_impl != NULL;
           iface_impl = iface_impl->mixin_next.priv)
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
     const gchar *name)
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
      interface_name);

  if (iface_impl == NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "No properties known for that interface" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  iface_info = iface_impl->mixin_priv;

  prop_impl = _tp_dbus_properties_mixin_find_prop_impl (iface_impl,
      property_name);

  if (prop_impl == NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "Unknown property" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  prop_info = prop_impl->mixin_priv;

  if ((prop_info->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_READ) == 0)
    {
      GError e = { TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
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
      interface_name);

  if (iface_impl == NULL)
    goto out;   /* no properties, but we need to return that */

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

out:
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
      interface_name);

  if (iface_impl == NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "No properties known for that interface" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  iface_info = iface_impl->mixin_priv;

  prop_impl = _tp_dbus_properties_mixin_find_prop_impl (iface_impl,
      property_name);

  if (prop_impl == NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "Unknown property" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  prop_info = prop_impl->mixin_priv;

  if ((prop_info->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_WRITE) == 0)
    {
      GError e = { TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
          "This property is read-only" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  if (G_VALUE_TYPE (value) != prop_info->type)
    {
      g_value_init (&copy, prop_info->type);

      if (!g_value_transform (value, &copy))
        {
          error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
              "Cannot convert %s to %s for property %s",
              g_type_name (G_VALUE_TYPE (value)),
              g_type_name (prop_info->type),
              property_name);

          dbus_g_method_return_error (context, error);
          g_error_free (error);
          goto out;
        }

      /* use copy instead of value from now on */
      value = &copy;
    }

  if (iface_impl->setter (self, iface_info->dbus_interface,
        prop_info->name, value, prop_impl->setter_data, &error))
    {
      tp_svc_dbus_properties_return_from_set (context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }

out:
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
