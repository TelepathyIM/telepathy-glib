/*
 * Copyright © 2014 Collabora Ltd.
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
#include <telepathy-glib/svc-interface-skeleton-internal.h>

#include <dbus/dbus-glib.h>

#include <telepathy-glib/asv.h>
#include <telepathy-glib/core-dbus-properties-mixin-internal.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/variant-util.h>

#define DEBUG(format, ...) \
  g_log (G_LOG_DOMAIN "/svc", G_LOG_LEVEL_DEBUG, "%s: " format, \
      G_STRFUNC, ##__VA_ARGS__)

struct _TpSvcInterfaceSkeletonPrivate
{
  GWeakRef object;
  TpSvcInterfaceInfo *iinfo;
};

G_DEFINE_TYPE (TpSvcInterfaceSkeleton, _tp_svc_interface_skeleton,
    G_TYPE_DBUS_INTERFACE_SKELETON)

static void
_tp_svc_interface_skeleton_init (TpSvcInterfaceSkeleton *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_SVC_INTERFACE_SKELETON, TpSvcInterfaceSkeletonPrivate);
}

static void
tp_svc_interface_skeleton_dispose (GObject *obj)
{
  TpSvcInterfaceSkeleton *self = TP_SVC_INTERFACE_SKELETON (obj);

  /* not using g_weak_ref_clear() in order to be idempotent */
  g_weak_ref_set (&self->priv->object, NULL);

  G_OBJECT_CLASS (_tp_svc_interface_skeleton_parent_class)->dispose (obj);
}

static GDBusInterfaceInfo *
tp_svc_interface_skeleton_get_info (GDBusInterfaceSkeleton *skel)
{
  TpSvcInterfaceSkeleton *self = TP_SVC_INTERFACE_SKELETON (skel);

  return self->priv->iinfo->interface_info;
}

static void
tp_svc_interface_skeleton_method_call (GDBusConnection *connection,
    const gchar *sender,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *method_name,
    GVariant *parameters,
    GDBusMethodInvocation *invocation,
    gpointer user_data)
{
  TpSvcInterfaceSkeleton *self = TP_SVC_INTERFACE_SKELETON (user_data);
  GObject *object;

  DEBUG ("%s.%s on %s %p from %s", interface_name, method_name, object_path,
      self, sender);

  object = g_weak_ref_get (&self->priv->object);
  /* TpDBusDaemon is meant to unexport us automatically when the object
   * goes away */
  g_return_if_fail (object != NULL);

  self->priv->iinfo->vtable->method_call (connection,
      sender, object_path, interface_name, method_name, parameters,
      invocation, object);
  g_object_unref (object);
}

static GVariant *
tp_svc_interface_skeleton_get_property (GDBusConnection *connection,
    const gchar *sender,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *property_name,
    GError **error,
    gpointer user_data)
{
  TpSvcInterfaceSkeleton *self = TP_SVC_INTERFACE_SKELETON (user_data);
  GObject *object;
  GVariant *ret = NULL;

  DEBUG ("Get(%s.%s) on %s %p from %s", interface_name, property_name,
      object_path, self, sender);

  object = g_weak_ref_get (&self->priv->object);
  g_return_val_if_fail (object != NULL, NULL);

  ret = _tp_dbus_properties_mixin_dup_in_dbus_lib (object, interface_name,
      property_name, error);

  g_object_unref (object);

  return ret;
}

static gboolean
tp_svc_interface_skeleton_set_property (GDBusConnection *connection,
    const gchar *sender,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *property_name,
    GVariant *variant,
    GError **error,
    gpointer user_data)
{
  TpSvcInterfaceSkeleton *self = TP_SVC_INTERFACE_SKELETON (user_data);
  GObject *object;
  gboolean ret;

  DEBUG ("Set(%s.%s) on %s %p from %s", interface_name, property_name,
      object_path, self, sender);

  object = g_weak_ref_get (&self->priv->object);
  g_return_val_if_fail (object != NULL, FALSE);

  ret = _tp_dbus_properties_mixin_set_in_dbus_lib (object, interface_name,
      property_name, variant, error);

  g_object_unref (object);

  return ret;
}

static GDBusInterfaceVTable vtable = {
    tp_svc_interface_skeleton_method_call,
    tp_svc_interface_skeleton_get_property,
    tp_svc_interface_skeleton_set_property
};

static GDBusInterfaceVTable *
tp_svc_interface_skeleton_get_vtable (GDBusInterfaceSkeleton *skel)
{
  return &vtable;
}

static GVariant *
tp_svc_interface_skeleton_get_properties (GDBusInterfaceSkeleton *skel)
{
  TpSvcInterfaceSkeleton *self = TP_SVC_INTERFACE_SKELETON (skel);
  const gchar *iface_name = self->priv->iinfo->interface_info->name;
  GObject *object;
  GVariant *ret;

  object = g_weak_ref_get (&self->priv->object);
  g_return_val_if_fail (object != NULL, NULL);

  /* For now assume we have the TpDBusPropertiesMixin if we have
   * any properties at all. This never returns NULL. */

  ret = _tp_dbus_properties_mixin_dup_all_in_dbus_lib (object, iface_name);
  g_object_unref (object);
  return ret;
}

static void
tp_svc_interface_skeleton_flush (GDBusInterfaceSkeleton *skel)
{
  /* stub: we emit any changes immediately, and we implement Properties
   * elsewhere anyway */
}

static void
_tp_svc_interface_skeleton_class_init (TpSvcInterfaceSkeletonClass *cls)
{
  GObjectClass *obj_cls = G_OBJECT_CLASS (cls);
  GDBusInterfaceSkeletonClass *skel_cls = G_DBUS_INTERFACE_SKELETON_CLASS (cls);

  g_type_class_add_private (cls, sizeof (TpSvcInterfaceSkeleton));

  obj_cls->dispose = tp_svc_interface_skeleton_dispose;

  skel_cls->get_info = tp_svc_interface_skeleton_get_info;
  skel_cls->get_vtable = tp_svc_interface_skeleton_get_vtable;
  skel_cls->get_properties = tp_svc_interface_skeleton_get_properties;
  skel_cls->flush = tp_svc_interface_skeleton_flush;
}

typedef struct {
    GClosure closure;
    /* (transfer none) - the closure ensures we have a ref */
    TpSvcInterfaceSkeleton *self;
    /* (transfer none) - borrowed from the interface info */
    const gchar *name;
} SignalClosure;

static void
tp_svc_interface_skeleton_emit_signal (GClosure *closure,
    GValue *return_value,
    guint n_param_values,
    const GValue *param_values,
    gpointer invocation_hint,
    gpointer marshal_data)
{
  SignalClosure *sc = (SignalClosure *) closure;
  TpSvcInterfaceSkeleton *self = sc->self;
  GDBusInterfaceSkeleton *skel = G_DBUS_INTERFACE_SKELETON (self);
  GDBusConnection *connection =
    g_dbus_interface_skeleton_get_connection (skel);
  const gchar *path = g_dbus_interface_skeleton_get_object_path (skel);
  GVariantBuilder builder;
  guint i;

  DEBUG ("%s.%s from %s %p", self->priv->iinfo->interface_info->name,
      sc->name, path, self);

  if (path == NULL || connection == NULL)
    {
      DEBUG ("- ignoring, object no longer exported");
      return;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);

  /* Skip parameter 0, which is the GObject. */
  for (i = 1; i < n_param_values; i++)
    g_variant_builder_add_value (&builder,
        dbus_g_value_build_g_variant (param_values + i));

  /* we only support being exported on one connection */
  g_dbus_connection_emit_signal (connection,
      NULL, /* broadcast */
      path,
      self->priv->iinfo->interface_info->name,
      sc->name,
      /* consume floating ref */
      g_variant_builder_end (&builder),
      /* cannot fail unless a parameter is incompatible with D-Bus,
       * so ignore error */
      NULL);
}

/**
 * tp_svc_interface_skeleton_new: (skip)
 * @object: (type GObject.Object): a #GObject
 * @iface: a `TpSvc` interface on the object
 *
 * Return a GDBus interface skeleton whose methods and signals
 * are implemented by @iface on @object, and whose properties
 * are implemented by a #TpDBusPropertiesMixin on @object.
 *
 * Returns: (transfer full): a new interface skeleton wrapping @iface
 *  on @object
 */
GDBusInterfaceSkeleton *
tp_svc_interface_skeleton_new (gpointer object,
    GType iface)
{
  TpSvcInterfaceSkeleton *self;
  const TpSvcInterfaceInfo *iinfo =
    tp_svc_interface_peek_dbus_interface_info (iface);

  g_return_val_if_fail (iinfo != NULL, NULL);

  /* not bothering to refcount it, it must be static for now */
  g_return_val_if_fail (iinfo->ref_count == -1, NULL);

  self = g_object_new (TP_TYPE_SVC_INTERFACE_SKELETON,
      NULL);
  g_weak_ref_init (&self->priv->object, object);
  self->priv->iinfo = (TpSvcInterfaceInfo *) iinfo;

  if (iinfo->signals != NULL)
    {
      guint i;

      for (i = 0; iinfo->signals[i] != NULL; i++)
        {
          const gchar *glib_name;
          const gchar *dbus_name;
          SignalClosure *closure;

          g_assert (iinfo->interface_info->signals[i] != NULL);

          glib_name = iinfo->signals[i];
          dbus_name = iinfo->interface_info->signals[i]->name;

          closure = (SignalClosure *) g_closure_new_object (sizeof (*closure),
              (GObject *) self);
          g_closure_set_marshal ((GClosure *) closure,
              tp_svc_interface_skeleton_emit_signal);
          closure->self = self;
          closure->name = dbus_name;

          g_signal_connect_closure (object, glib_name, (GClosure *) closure,
              FALSE);
        }
    }

  return G_DBUS_INTERFACE_SKELETON (self);
}
