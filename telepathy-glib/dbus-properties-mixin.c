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

#include <telepathy-glib/asv.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/sliced-gvalue.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/svc-interface.h>
#include <telepathy-glib/util.h>

#include "telepathy-glib/dbus-internal.h"
#include <telepathy-glib/object-registration-internal.h>

/* no debug infrastructure here, this is the -dbus library */
#define DEBUG(format, ...) \
  g_log (G_LOG_DOMAIN "/properties", G_LOG_LEVEL_DEBUG, "%s: " format, \
      G_STRFUNC, ##__VA_ARGS__)
#define CRITICAL(format, ...) \
  g_log (G_LOG_DOMAIN "/properties", G_LOG_LEVEL_CRITICAL, "%s: " format, \
      G_STRFUNC, ##__VA_ARGS__)
#define WARNING(format, ...) \
  g_log (G_LOG_DOMAIN "/properties", G_LOG_LEVEL_WARNING, "%s: " format, \
      G_STRFUNC, ##__VA_ARGS__)

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
 * or call tp_svc_interface_set_dbus_properties_info() from a section of the
 * base_init function that only runs once.
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
 * @TP_DBUS_PROPERTIES_MIXIN_FLAG_EMITS_CHANGED: The property's new value is
 *  included in emissions of PropertiesChanged
 * @TP_DBUS_PROPERTIES_MIXIN_FLAG_EMITS_INVALIDATED: The property is announced
 *  as invalidated, without its value, in emissions of PropertiesChanged
 *
 * Bitfield representing allowed access to a property. At most one of
 * %TP_DBUS_PROPERTIES_MIXIN_FLAG_EMITS_CHANGED and
 * %TP_DBUS_PROPERTIES_MIXIN_FLAG_EMITS_INVALIDATED may be specified for a
 * property.
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

/**
 * TpDBusPropertiesMixinGetter:
 * @object: The exported object with the properties
 * @iface: A quark representing the D-Bus interface name
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
 * @iface: A quark representing the D-Bus interface name
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
                                                    GQuark iface,
                                                    GQuark name,
                                                    GValue *value,
                                                    gpointer getter_data)
{
  g_object_get_property (object, getter_data, value);
}

/**
 * TpDBusPropertiesMixinSetter:
 * @object: The exported object with the properties
 * @iface: A quark representing the D-Bus interface name
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
 * @iface: A quark representing the D-Bus interface name
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
                                                    GQuark iface,
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
      CRITICAL ("%s tried to implement undefined interface %s "
          "(perhaps you forgot to call G_IMPLEMENT_INTERFACE?)",
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
          CRITICAL ("%s tried to implement nonexistent property %s"
              " on interface %s", g_type_name (type), prop_impl->name,
              iface_impl->name);
          return FALSE;
        }
    }

  return TRUE;
}

/* if this assertion fails, TpDBusPropertiesMixinIfaceImpl.mixin_next (which
 * used to be a GCallback but is now a gpointer) will be an ABI break on this
 * architecture, so do some evil trick with unions or something */
G_STATIC_ASSERT (sizeof (GCallback) == sizeof (gpointer));

/* FIXME: GNOME#556489: getter and setter should be (scope infinite) if that
 * existed */
/**
 * tp_dbus_properties_mixin_implement_interface: (skip)
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
  GQuark extras_quark = _extra_prop_impls_quark ();
  GType type = G_OBJECT_CLASS_TYPE (cls);
  GType *interfaces = g_type_interfaces (type, NULL);
  TpDBusPropertiesMixinIfaceImpl *iface_impl;

  g_return_if_fail (G_IS_OBJECT_CLASS (cls));

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
      GQuark offset_quark = _prop_mixin_offset_quark ();
      gpointer offset_qdata = g_type_get_qdata (type, offset_quark);
      TpDBusPropertiesMixinClass *mixin = NULL;
      TpDBusPropertiesMixinIfaceImpl *iter;

      if (offset_qdata != NULL)
        mixin = &G_STRUCT_MEMBER (TpDBusPropertiesMixinClass, cls,
            GPOINTER_TO_SIZE (offset_qdata));

      /* assert that we're not trying to implement the same interface twice */
      for (iter = next;
           iter != NULL && iter->name != NULL;
           iter = iter->mixin_next)
        {
          TpDBusPropertiesMixinIfaceInfo *other_info = iter->mixin_priv;

          g_assert (other_info != NULL);

          if (G_UNLIKELY (other_info->dbus_interface == iface))
            {
              CRITICAL ("type %s tried to implement interface %s with %s "
                  "twice", g_type_name (type), g_quark_to_string (iface),
                  G_STRFUNC);
              goto out;
            }
        }

      /* assert that we're not trying to implement the same interface via
       * this function and the static data */
      if (mixin != NULL && mixin->interfaces != NULL)
        {
          for (iter = mixin->interfaces;
               iter->name != NULL;
               iter++)
            {
              TpDBusPropertiesMixinIfaceInfo *other_info = iter->mixin_priv;

              g_assert (other_info != NULL);

              if (G_UNLIKELY (other_info->dbus_interface == iface))
                {
                  CRITICAL ("type %s tried to implement interface %s with %s "
                      "and also in static data", g_type_name (type),
                      g_quark_to_string (iface), G_STRFUNC);
                  goto out;
                }
            }
        }

      /* form a linked list */
      iface_impl->mixin_next = next;
      g_type_set_qdata (type, extras_quark, iface_impl);
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
      TpDBusPropertiesMixinClass *mixin = NULL;
      TpDBusPropertiesMixinIfaceImpl *iface_impl;
      TpDBusPropertiesMixinIfaceInfo *iface_info;

      if (offset != NULL)
        mixin = &G_STRUCT_MEMBER (TpDBusPropertiesMixinClass,
            G_OBJECT_GET_CLASS (self), GPOINTER_TO_SIZE (offset));

      if (mixin != NULL && mixin->interfaces != NULL)
        {
          for (iface_impl = mixin->interfaces;
               iface_impl->name != NULL;
               iface_impl++)
            {
              iface_info = iface_impl->mixin_priv;

              if (iface_info->dbus_interface == iface_quark)
                return iface_impl;
            }
        }

      for (iface_impl = g_type_get_qdata (type, extras_quark);
           iface_impl != NULL;
           iface_impl = iface_impl->mixin_next)
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

static TpDBusPropertiesMixinPropImpl *
_iface_impl_get_property_impl (
    GObject *self,
    TpDBusPropertiesMixinIfaceImpl *iface_impl,
    const gchar *interface_name,
    const gchar *property_name,
    GError **error)
{
  TpDBusPropertiesMixinPropImpl *prop_impl;
  TpDBusPropertiesMixinPropInfo *prop_info;

  prop_impl = _tp_dbus_properties_mixin_find_prop_impl (iface_impl,
      property_name);

  if (prop_impl == NULL)
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "Unknown property %s on %s", property_name, interface_name);
      return FALSE;
    }

  prop_info = prop_impl->mixin_priv;

  if ((prop_info->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_READ) == 0)
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "Property %s on %s is write-only", property_name, interface_name);
      return FALSE;
    }

  if (iface_impl->getter == NULL)
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "Getting properties on %s is unimplemented", interface_name);
      return FALSE;
    }

  return prop_impl;
}

/**
 * tp_dbus_properties_mixin_get:
 * @self: an object with this mixin
 * @interface_name: a D-Bus interface name
 * @property_name: a D-Bus property name
 * @value: an unset GValue (initialized to all zeroes)
 * @error: used to return an error on failure
 *
 * Initialize @value with the type of the property @property_name on
 * @interface_name, and write the value of that property into it as if
 * by calling the D-Bus method org.freedesktop.DBus.Properties.Get.
 *
 * If Get would return a D-Bus error, @value remains unset and @error
 * is filled in instead.
 *
 * Returns: %TRUE (filling @value) on success, %FALSE (setting @error)
 *  on failure
 * Since: 0.7.13
 */
gboolean
tp_dbus_properties_mixin_get (GObject *self,
                              const gchar *interface_name,
                              const gchar *property_name,
                              GValue *value,
                              GError **error)
{
  TpDBusPropertiesMixinIfaceImpl *iface_impl;
  TpDBusPropertiesMixinPropImpl *prop_impl;

  g_return_val_if_fail (G_IS_OBJECT (self), FALSE);
  g_return_val_if_fail (interface_name != NULL, FALSE);
  g_return_val_if_fail (property_name != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  iface_impl = _tp_dbus_properties_mixin_find_iface_impl (self,
      interface_name);

  if (iface_impl == NULL)
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "No properties known for interface %s", interface_name);
      return FALSE;
    }

  prop_impl = _iface_impl_get_property_impl (self, iface_impl, interface_name,
      property_name, error);

  if (prop_impl != NULL)
    {
      TpDBusPropertiesMixinIfaceInfo *iface_info = iface_impl->mixin_priv;
      TpDBusPropertiesMixinPropInfo *prop_info = prop_impl->mixin_priv;

      g_value_init (value, prop_info->type);
      iface_impl->getter (self, iface_info->dbus_interface,
          prop_info->name, value, prop_impl->getter_data);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}


static void
tp_dbus_properties_mixin_fill_properties_hash_va (
    GObject *object,
    GHashTable *table,
    const gchar *first_interface,
    const gchar *first_property,
    va_list ap)
{
  const gchar *iface, *property;
  gboolean first = TRUE;

  for (iface = first_interface;
       iface != NULL;
       iface = va_arg (ap, gchar *))
    {
      GValue *value = g_slice_new0 (GValue);
      GError *error = NULL;

      if (first)
        {
          property = first_property;
          first = FALSE;
        }
      else
        {
          property = va_arg (ap, gchar *);
        }

      /* If property is NULL, the caller might have omitted a comma or
       * something; in any case, it shouldn't be.
       */
      g_assert (property != NULL);

      if (tp_dbus_properties_mixin_get (object, iface, property, value,
              &error))
        {
          g_assert (G_IS_VALUE (value));
          g_hash_table_insert (table,
              g_strdup_printf ("%s.%s", iface, property), value);
        }
      else
        {
          /* This is bad and definitely indicates a programming error. */
          CRITICAL ("Couldn't fetch '%s' on interface '%s': %s",
              property, iface, error->message);
          g_clear_error (&error);
        }

    }
}

/**
 * tp_dbus_properties_mixin_fill_properties_hash: (skip)
 * @object: an object which uses the D-Bus properties mixin
 * @table: (element-type utf8 GObject.Value): a hash table where the keys are
 *  strings copied with g_strdup() and the values are slice-allocated
 *  #GValue<!-- -->s
 * @first_interface: the interface of the first property to be retrieved
 * @first_property: the name of the first property to be retrieved
 * @...: more (interface name, property name) pairs, terminated by %NULL.
 *
 * Retrieves the values of several D-Bus properties from an object, and adds
 * them to a hash mapping the fully-qualified name of the property to its
 * value. This is equivalent to calling tp_dbus_properties_mixin_get() for
 * each property and adding it to the table yourself, with the proviso that
 * this function will g_assert() if retrieving a property fails (for instance,
 * because it does not exist).
 *
 * Note that in particular, @table does not have the same memory-allocation
 * model as the hash tables required by tp_asv_set_string() and similar
 * functions.
 *
 * Since: 0.11.11
 */
void
tp_dbus_properties_mixin_fill_properties_hash (
    GObject *object,
    GHashTable *table,
    const gchar *first_interface,
    const gchar *first_property,
    ...)
{
  va_list ap;

  va_start (ap, first_property);
  tp_dbus_properties_mixin_fill_properties_hash_va (object, table,
      first_interface, first_property, ap);
  va_end (ap);
}

/**
 * tp_dbus_properties_mixin_make_properties_hash: (skip)
 * @object: an object which uses the D-Bus properties mixin
 * @first_interface: the interface of the first property to be retrieved
 * @first_property: the name of the first property to be retrieved
 * @...: more (interface name, property name) pairs, terminated by %NULL.
 *
 * Retrieves the values of several D-Bus properties from an object, and builds
 * a hash mapping the fully-qualified name of the property to its value.  This
 * is equivalent to calling tp_dbus_properties_mixin_get() for each property
 * and building the table yourself, with the proviso that this function will
 * g_assert() if retrieving a property fails (for instance, because it does not
 * exist).
 *
 * Additional keys and values can be inserted into the returned hash table;
 * if this is done, the inserted keys and values will be freed when the
 * hash table is destroyed. The keys must be allocated with g_strdup() or
 * equivalent, and the values must be slice-allocated (for instance with
 * tp_g_value_slice_new_string() or a similar function).
 *
 * Note that in particular, tp_asv_set_string() and similar functions should
 * not be used with this hash table.
 *
 * Returns: a hash table mapping (gchar *) fully-qualified property names to
 *          GValues, which must be freed by the caller (at which point its
 *          contents will also be freed).
 */
GHashTable *
tp_dbus_properties_mixin_make_properties_hash (
    GObject *object,
    const gchar *first_interface,
    const gchar *first_property,
    ...)
{
  GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) tp_g_value_slice_free);
  va_list ap;

  va_start (ap, first_property);
  tp_dbus_properties_mixin_fill_properties_hash_va (object, table,
      first_interface, first_property, ap);
  va_end (ap);

  return table;
}

/**
 * tp_dbus_properties_mixin_emit_properties_changed:
 * @object: an object which uses the D-Bus properties mixin
 * @interface_name: the interface on which properties have changed
 * @properties: (allow-none): a %NULL-terminated array of (unqualified)
 *  property names whose values have changed.
 *
 * Emits the PropertiesChanged signal for the provided properties. Depending on
 * the EmitsChangedSignal annotations in the introspection XML, either the new
 * value of the property will be included in the signal, or merely the fact
 * that the property has changed.
 *
 * For example, the MPRIS specification defines a TrackList interface with two
 * properties, one of which is annotated with EmitsChangedSignal=true and one
 * annotated with EmitsChangedSignal=invalidates. The following call would
 * include the new value of CanEditTracks and list Tracks as invalidated:
 *
 * |[
 *    const gchar *properties[] = { "CanEditTracks", "Tracks", NULL };
 *
 *    tp_dbus_properties_mixin_emit_properties_changed (G_OBJECT (self),
 *        "org.mpris.MediaPlayer2.TrackList", properties);
 * ]|
 *
 * It is an error to pass a property to this
 * function if the property is annotated with EmitsChangedSignal=false, or is
 * unannotated.
 *
 * Since: 0.15.6
 */
void
tp_dbus_properties_mixin_emit_properties_changed (
    GObject *object,
    const gchar *interface_name,
    const gchar * const *properties)
{
  TpDBusPropertiesMixinIfaceImpl *iface_impl;
  TpDBusPropertiesMixinIfaceInfo *iface_info;
  GVariantDict changed_properties;
  GPtrArray *invalidated_properties;
  const gchar * const *prop_name;
  TpDBusConnectionRegistration *r;

  g_return_if_fail (interface_name != NULL);
  iface_impl = _tp_dbus_properties_mixin_find_iface_impl (object,
      interface_name);
  g_return_if_fail (iface_impl != NULL);

  iface_info = iface_impl->mixin_priv;

  /* If someone passes no property names, well … that's fine, we have nothing
   * to do.
   */
  if (properties == NULL || properties[0] == NULL)
    return;

  g_variant_dict_init (&changed_properties, NULL);
  invalidated_properties = g_ptr_array_new ();

  for (prop_name = properties; *prop_name != NULL; prop_name++)
    {
      TpDBusPropertiesMixinPropImpl *prop_impl;
      TpDBusPropertiesMixinPropInfo *prop_info;
      GError *error = NULL;

      prop_impl = _iface_impl_get_property_impl (object, iface_impl,
          interface_name, *prop_name, &error);

      if (prop_impl == NULL)
        {
          WARNING ("Couldn't get value for '%s.%s': %s", interface_name,
              *prop_name, error->message);
          g_clear_error (&error);
          g_return_if_reached ();
        }

      prop_info = prop_impl->mixin_priv;

      if (prop_info->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_EMITS_CHANGED)
        {
          GValue v = { 0, };
          GVariant *variant;

          g_value_init (&v, prop_info->type);
          iface_impl->getter (object, iface_info->dbus_interface,
              prop_info->name, &v, prop_impl->getter_data);
          variant = dbus_g_value_build_g_variant (&v);
          g_variant_dict_insert_value (&changed_properties, *prop_name,
              variant);

          g_value_unset (&v);
        }
      else if (prop_info->flags &
                  TP_DBUS_PROPERTIES_MIXIN_FLAG_EMITS_INVALIDATED)
        {
          g_ptr_array_add (invalidated_properties, (gchar *) *prop_name);
        }
      else
        {
          WARNING ("'%s.%s' is not annotated with EmitsChangedSignal'",
              interface_name, *prop_name);
        }
    }

  g_ptr_array_add (invalidated_properties, NULL);

  r = g_object_get_qdata (object, _tp_dbus_connection_registration_quark ());

  if (r != NULL && r->conn != NULL)
    {
      g_dbus_connection_emit_signal (r->conn,
          NULL, /* broadcast */
          r->object_path,
          "org.freedesktop.DBus.Properties",
          "PropertiesChanged",
          /* consume floating ref */
          g_variant_new ("(s@a{sv}^as)", interface_name,
              g_variant_dict_end (&changed_properties),
              invalidated_properties->pdata),
          /* cannot fail unless a parameter is incompatible with D-Bus,
           * so ignore error */
          NULL);
    }

  g_ptr_array_unref (invalidated_properties);
}

/**
 * tp_dbus_properties_mixin_emit_properties_changed_varargs: (skip)
 * @object: an object which uses the D-Bus properties mixin
 * @interface_name: the interface on which properties have changed
 * @...: property names (unqualified) whose values have changed, terminated by
 *  %NULL.
 *
 * A shortcut for calling tp_dbus_properties_mixin_emit_properties_changed().
 *
 * Since: 0.15.6
 */
void
tp_dbus_properties_mixin_emit_properties_changed_varargs (
    GObject *object,
    const gchar *interface_name,
    ...)
{
  GPtrArray *property_names = g_ptr_array_new ();
  char *property_name;
  va_list ap;

  va_start (ap, interface_name);
  do
    {
      property_name = va_arg (ap, char *);
      g_ptr_array_add (property_names, property_name);
    }
  while (property_name != NULL);
  va_end (ap);

  tp_dbus_properties_mixin_emit_properties_changed (object, interface_name,
      (const gchar * const *) property_names->pdata);
  g_ptr_array_unref (property_names);
}

/**
 * tp_dbus_properties_mixin_dup_all:
 * @self: an object with this mixin
 * @interface_name: a D-Bus interface name
 *
 * Get all the properties of a particular interface. This implementation
 * never returns an error: it will return an empty map if the interface
 * is unknown.
 *
 * Returns: (transfer container) (element-type utf8 GObject.Value): a map
 *  from property name (without the interface name) to value
 * Since: 0.21.2
 */
GHashTable *
tp_dbus_properties_mixin_dup_all (GObject *self,
    const gchar *interface_name)
{
  TpDBusPropertiesMixinIfaceImpl *iface_impl;
  TpDBusPropertiesMixinIfaceInfo *iface_info;
  TpDBusPropertiesMixinPropImpl *prop_impl;
  /* no key destructor needed - the keys are immortal */
  GHashTable *values = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

  iface_impl = _tp_dbus_properties_mixin_find_iface_impl (self,
      interface_name);

  if (iface_impl == NULL || iface_impl->getter == NULL)
    return values;

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

  return values;
}

/**
 * tp_dbus_properties_mixin_set:
 * @self: an object with this mixin
 * @interface_name: a D-Bus interface name
 * @property_name: a D-Bus property name
 * @value: a GValue containing the new value for this property.
 * @error: used to return an error on failure
 *
 * Sets a property to the value specified by @value, as if by
 * calling the D-Bus method org.freedesktop.DBus.Properties.Set.
 *
 * If Set would return a D-Bus error, sets @error and returns %FALSE
 *
 * Returns: %TRUE on success; %FALSE (setting @error) on failure
 * Since: 0.15.8
 */
gboolean
tp_dbus_properties_mixin_set (
    GObject *self,
    const gchar *interface_name,
    const gchar *property_name,
    const GValue *value,
    GError **error)
{
  TpDBusPropertiesMixinIfaceImpl *iface_impl;
  TpDBusPropertiesMixinIfaceInfo *iface_info;
  TpDBusPropertiesMixinPropImpl *prop_impl;
  TpDBusPropertiesMixinPropInfo *prop_info;
  GValue copy = { 0 };
  gboolean ret;

  g_return_val_if_fail (G_IS_OBJECT (self), FALSE);
  g_return_val_if_fail (interface_name != NULL, FALSE);
  g_return_val_if_fail (property_name != NULL, FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);

  iface_impl = _tp_dbus_properties_mixin_find_iface_impl (self,
      interface_name);

  if (iface_impl == NULL)
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "No properties known for interface '%s'", interface_name);
      return FALSE;
    }

  iface_info = iface_impl->mixin_priv;

  prop_impl = _tp_dbus_properties_mixin_find_prop_impl (iface_impl,
      property_name);

  if (prop_impl == NULL)
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "Unknown property '%s' on interface '%s'", property_name,
          interface_name);
      return FALSE;
    }

  prop_info = prop_impl->mixin_priv;

  if ((prop_info->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_WRITE) == 0)
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "'%s.%s' is read-only", interface_name, property_name);
      return FALSE;
    }

  if (iface_impl->setter == NULL)
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "Setting properties on '%s' is unimplemented", interface_name);
      return FALSE;
    }

  if (G_VALUE_TYPE (value) != prop_info->type)
    {
      g_value_init (&copy, prop_info->type);

      if (!g_value_transform (value, &copy))
        {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
              "Cannot convert %s to %s for property %s",
              g_type_name (G_VALUE_TYPE (value)),
              g_type_name (prop_info->type),
              property_name);
          ret = FALSE;
          goto out;
        }

      /* use copy instead of value from now on */
      value = &copy;
    }

  ret = iface_impl->setter (self, iface_info->dbus_interface,
        prop_info->name, value, prop_impl->setter_data, error);

out:
  if (G_IS_VALUE (&copy))
    g_value_unset (&copy);

  return ret;
}

/**
 * tp_dbus_properties_mixin_dup_variant:
 * @object: an object with this mixin
 * @interface_name: a D-Bus interface name
 * @property_name: a D-Bus property name
 * @error: used to return an error on failure
 *
 * Get the value of the property @property_name on @interface_name, as if
 * by calling the D-Bus method org.freedesktop.DBus.Properties.Get.
 *
 * If Get would return a D-Bus error, %NULL is returned and @error
 * is filled in instead.
 *
 * Returns: (transfer full) (allow-none): a reference to a #GVariant, or %NULL
 */
GVariant *
tp_dbus_properties_mixin_dup_variant (GObject *object,
    const gchar *interface_name,
    const gchar *property_name,
    GError **error)
{
  GValue value = G_VALUE_INIT;
  GVariant *ret = NULL;

  if (tp_dbus_properties_mixin_get (object, interface_name, property_name,
          &value, error))
    {
      ret = g_variant_ref_sink (dbus_g_value_build_g_variant (&value));
      g_value_unset (&value);
    }

  return ret;
}

/**
 * tp_dbus_properties_mixin_set_variant:
 * @object: an object with this mixin
 * @interface_name: a D-Bus interface name
 * @property_name: a D-Bus property name
 * @value: a #GVariant containing the new value for this property;
 *  if it is floating, ownership will be taken
 * @error: used to return an error on failure
 *
 * Sets a property to the value specified by @value, as if by
 * calling the D-Bus method org.freedesktop.DBus.Properties.Set.
 *
 * If Set would return a D-Bus error, sets @error and returns %FALSE
 *
 * Returns: %TRUE on success; %FALSE (setting @error) on failure
 */
gboolean
tp_dbus_properties_mixin_set_variant (GObject *object,
    const gchar *interface_name,
    const gchar *property_name,
    GVariant *value,
    GError **error)
{
  gboolean ret;
  GValue gvalue = G_VALUE_INIT;

  g_variant_ref_sink (value);

  dbus_g_value_parse_g_variant (value, &gvalue);
  ret = tp_dbus_properties_mixin_set (object, interface_name, property_name,
      &gvalue, error);

  g_value_unset (&gvalue);
  g_variant_unref (value);
  return ret;
}

/**
 * tp_dbus_properties_mixin_dup_all_vardict:
 * @object: an object with this mixin
 * @interface_name: a D-Bus interface name
 *
 * Get all the properties of a particular interface, as if by
 * calling the D-Bus method org.freedesktop.DBus.Properties.GetAll.
 * This implementation
 * never returns an error: it will return an empty map if the interface
 * is unknown.
 *
 * Returns: (transfer full): a variant of type %G_VARIANT_TYPE_VARDICT
 */
GVariant *
tp_dbus_properties_mixin_dup_all_vardict (GObject *object,
    const gchar *interface_name)
{
  GVariant *ret;
  GHashTable *asv;

  asv = tp_dbus_properties_mixin_dup_all (object, interface_name);
  ret = g_variant_ref_sink (tp_asv_to_vardict (asv));
  g_hash_table_unref (asv);
  return ret;
}
