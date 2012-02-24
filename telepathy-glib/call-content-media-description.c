/*
 * call-content-media-description.c - Source for TpyCallContentMediaDescription
 * Copyright (C) 2009-2011 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.com>
 * @author Olivier Crete <olivier.crete@collabora.com>
 * @author Xavier Claessens <xavier.claessens@collabora.co.uk>
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
 * SECTION:call-content-media-description
 * @title: TpCallContentMediaDescription
 * @short_description: implementation of #TpSvcCallContentMediaDescription
 * @see_also: #TpBaseMediaCallContent
 *
 * This class is used to negociate the media description used with a remote
 * contact. To be used with #TpBaseMediaCallContent implementations.
 *
 * Since: 0.17.5
 */

/**
 * TpCallContentMediaDescription:
 *
 * A class for media content description
 *
 * Since: 0.17.5
 */

/**
 * TpCallContentMediaDescriptionClass:
 *
 * The class structure for #TpCallContentMediaDescription
 *
 * Since: 0.17.5
 */

#include "config.h"

#include "call-content-media-description.h"

#define DEBUG_FLAG TP_DEBUG_CALL
#include "telepathy-glib/base-call-internal.h"
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/dbus.h"
#include "telepathy-glib/gtypes.h"
#include "telepathy-glib/handle.h"
#include "telepathy-glib/interfaces.h"
#include "telepathy-glib/svc-call.h"
#include "telepathy-glib/svc-properties-interface.h"
#include "telepathy-glib/util.h"
#include "telepathy-glib/util-internal.h"

static void call_content_media_description_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE(TpCallContentMediaDescription,
  tp_call_content_media_description,
  G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CALL_CONTENT_MEDIA_DESCRIPTION,
        call_content_media_description_iface_init);
   G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
    tp_dbus_properties_mixin_iface_init);
  );

static const gchar *tp_call_content_media_description_interfaces[] = {
    NULL
};

/* properties */
enum
{
  PROP_OBJECT_PATH = 1,
  PROP_DBUS_DAEMON,

  PROP_INTERFACES,
  PROP_FURTHER_NEGOTIATION_REQUIRED,
  PROP_HAS_REMOTE_INFORMATION,
  PROP_CODECS,
  PROP_REMOTE_CONTACT,
  PROP_SSRCS
};

/* private structure */
struct _TpCallContentMediaDescriptionPrivate
{
  TpDBusDaemon *dbus_daemon;
  gchar *object_path;

  gboolean further_negotiation_required;
  gboolean has_remote_information;
  /* GPtrArray of owned GValueArray */
  GPtrArray *codecs;
  TpHandle remote_contact;
  /* TpHandle -> reffed GArray<uint> */
  GHashTable *ssrcs;

  GSimpleAsyncResult *result;
  GCancellable *cancellable;
  guint handler_id;
};

static void
tp_call_content_media_description_init (TpCallContentMediaDescription *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_CALL_CONTENT_MEDIA_DESCRIPTION,
      TpCallContentMediaDescriptionPrivate);

  self->priv->ssrcs = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_array_unref);
  self->priv->codecs = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_value_array_free);
}

static void
tp_call_content_media_description_dispose (GObject *object)
{
  TpCallContentMediaDescription *self = (TpCallContentMediaDescription *) object;

  g_assert (self->priv->result == NULL);

  tp_clear_pointer (&self->priv->codecs, g_ptr_array_unref);
  tp_clear_pointer (&self->priv->ssrcs, g_hash_table_unref);
  g_clear_object (&self->priv->dbus_daemon);

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (tp_call_content_media_description_parent_class)->dispose)
    G_OBJECT_CLASS (tp_call_content_media_description_parent_class)->dispose (
        object);
}

static void
tp_call_content_media_description_finalize (GObject *object)
{
  TpCallContentMediaDescription *self = (TpCallContentMediaDescription *) object;

  g_free (self->priv->object_path);

  G_OBJECT_CLASS (tp_call_content_media_description_parent_class)->finalize (
      object);
}

static void
tp_call_content_media_description_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpCallContentMediaDescription *self = (TpCallContentMediaDescription *) object;

  switch (property_id)
    {
      case PROP_OBJECT_PATH:
        g_value_set_string (value, self->priv->object_path);
        break;
      case PROP_DBUS_DAEMON:
        g_value_set_object (value, self->priv->dbus_daemon);
        break;
      case PROP_INTERFACES:
        g_value_set_boxed (value, tp_call_content_media_description_interfaces);
        break;
      case PROP_FURTHER_NEGOTIATION_REQUIRED:
        g_value_set_boolean (value, self->priv->further_negotiation_required);
        break;
      case PROP_HAS_REMOTE_INFORMATION:
        g_value_set_boolean (value, self->priv->has_remote_information);
        break;
      case PROP_CODECS:
        g_value_set_boxed (value, self->priv->codecs);
        break;
      case PROP_REMOTE_CONTACT:
        g_value_set_uint (value, self->priv->remote_contact);
        break;
      case PROP_SSRCS:
        g_value_set_boxed (value, self->priv->ssrcs);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tp_call_content_media_description_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpCallContentMediaDescription *self = (TpCallContentMediaDescription *) object;

  switch (property_id)
    {
      case PROP_OBJECT_PATH:
        g_assert (self->priv->object_path == NULL); /* construct-only */
        self->priv->object_path = g_value_dup_string (value);
        break;
      case PROP_DBUS_DAEMON:
        g_assert (self->priv->dbus_daemon == NULL); /* construct-only */
        self->priv->dbus_daemon = g_value_dup_object (value);
        break;
      case PROP_FURTHER_NEGOTIATION_REQUIRED:
        self->priv->further_negotiation_required = g_value_get_boolean (value);
        break;
      case PROP_HAS_REMOTE_INFORMATION:
        self->priv->has_remote_information = g_value_get_boolean (value);
        break;
      case PROP_REMOTE_CONTACT:
        self->priv->remote_contact = g_value_get_uint (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tp_call_content_media_description_class_init (
    TpCallContentMediaDescriptionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *spec;
  static TpDBusPropertiesMixinPropImpl media_description_props[] = {
    { "Interfaces", "interfaces", NULL },
    { "FurtherNegotiationRequired", "further-negotiation-required", NULL },
    { "HasRemoteInformation", "has-remote-information", NULL},
    { "Codecs", "codecs", NULL },
    { "RemoteContact", "remote-contact", NULL },
    { "SSRCs", "ssrcs", NULL },
    { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
      { TP_IFACE_CALL_CONTENT_MEDIA_DESCRIPTION,
        tp_dbus_properties_mixin_getter_gobject_properties,
        NULL,
        media_description_props,
      },
      { NULL }
  };

  g_type_class_add_private (klass, sizeof (TpCallContentMediaDescriptionPrivate));

  object_class->get_property = tp_call_content_media_description_get_property;
  object_class->set_property = tp_call_content_media_description_set_property;
  object_class->dispose = tp_call_content_media_description_dispose;
  object_class->finalize = tp_call_content_media_description_finalize;

  /**
   * TpCallContentMediaDescription:object-path:
   *
   * The D-Bus object path used for this object on the bus.
   *
   * Since: 0.17.5
   */
  spec = g_param_spec_string ("object-path", "D-Bus object path",
      "The D-Bus object path used for this "
      "object on the bus.",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, spec);

  /**
   * TpCallContentMediaDescription:dbus-daemon:
   *
   * The connection to the DBus daemon owning the CM.
   *
   * Since: 0.17.5
   */
  spec = g_param_spec_object ("dbus-daemon",
      "The DBus daemon connection",
      "The connection to the DBus daemon owning the CM",
      TP_TYPE_DBUS_DAEMON,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DBUS_DAEMON, spec);

  /**
   * TpCallContentMediaDescription:interfaces:
   *
   * Additional interfaces implemented by this object.
   *
   * Since: 0.17.5
   */
  spec = g_param_spec_boxed ("interfaces",
      "Interfaces",
      "Extra interfaces provided by this media description",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES,
      spec);

  /**
   * TpCallContentMediaDescription:further-negotiation-required:
   *
   * %TRUE if more negotiation is required after MediaDescription is processed.
   *
   * Since: 0.17.5
   */
  spec = g_param_spec_boolean ("further-negotiation-required",
      "FurtherNegotiationRequired",
      "More negotiation is required after MediaDescription is processed",
      FALSE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_FURTHER_NEGOTIATION_REQUIRED,
      spec);

  /**
   * TpCallContentMediaDescription:further-negotiation-required:
   *
   * %TRUE if the MediaDescription contains remote information.
   *
   * Since: 0.17.5
   */
  spec = g_param_spec_boolean ("has-remote-information",
      "HasRemoteInformation",
      "True if the MediaDescription contains remote information",
      FALSE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_HAS_REMOTE_INFORMATION,
      spec);

  /**
   * TpCallContentMediaDescription:codecs:
   *
   * #GPtrArray{codecs #GValueArray}.
   * A list of codecs the remote contact supports.
   *
   * Since: 0.17.5
   */
  spec = g_param_spec_boxed ("codecs",
      "Codecs",
      "A list of codecs the remote contact supports",
      TP_ARRAY_TYPE_CODEC_LIST,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CODECS,
      spec);

  /**
   * TpCallContentMediaDescription:remote-contact:
   *
   * The contact #TpHandle that this media description applies to.
   *
   * Since: 0.17.5
   */
  spec = g_param_spec_uint ("remote-contact",
      "RemoteContact",
      "The contact handle that this media description applies to",
      0, G_MAXUINT, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REMOTE_CONTACT,
      spec);

  /**
   * TpCallContentMediaDescription:ssrcs:
   *
   * #GHashTable{contact #TpHandle, #GArray{uint}}
   * A map of contacts to SSRCs.
   *
   * Since: 0.17.5
   */
  spec = g_param_spec_boxed ("ssrcs",
      "SSRCs",
      "A map of handles to SSRCs",
      TP_HASH_TYPE_CONTACT_SSRCS_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SSRCS, spec);

  klass->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpCallContentMediaDescriptionClass, dbus_props_class));
}

/**
 * tp_call_content_media_description_new:
 * @dbus_daemon: value of #TpCallContentMediaDescription:dbus-daemon property
 * @object_path: value of #TpCallContentMediaDescription:object-path property
 * @remote_contact: value of
 *  #TpCallContentMediaDescription:remote-contact property
 * @has_remote_information: value of
 *  #TpCallContentMediaDescription:has_remote_information property
 * @further_negotiation_required: value of
 *  #TpCallContentMediaDescription:further_negotiation_required property
 *
 * Create a new #TpCallContentMediaDescription object. More information can be
 * added after construction using
 * tp_call_content_media_description_append_codec() and
 * tp_call_content_media_description_add_ssrc().
 *
 * Once all information has been filled, the media description can be offered
 * using tp_base_media_call_content_offer_media_description().
 *
 * Returns: a new #TpCallContentMediaDescription.
 * Since: 0.17.5
 */
TpCallContentMediaDescription *
tp_call_content_media_description_new (TpDBusDaemon *dbus_daemon,
    const gchar *object_path,
    TpHandle remote_contact,
    gboolean has_remote_information,
    gboolean further_negotiation_required)
{
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);

  return g_object_new (TP_TYPE_CALL_CONTENT_MEDIA_DESCRIPTION,
      "dbus-daemon", dbus_daemon,
      "object-path", object_path,
      "further-negotiation-required", further_negotiation_required,
      "has-remote-information", has_remote_information,
      "remote-contact", remote_contact,
      NULL);
}

/**
 * tp_call_content_media_description_get_object_path:
 * @self: a #TpCallContentMediaDescription
 *
 * <!-- -->
 *
 * Returns: the value of #TpCallContentMediaDescription:object-path
 * Since: 0.17.5
 */
const gchar *
tp_call_content_media_description_get_object_path (
    TpCallContentMediaDescription *self)
{
  g_return_val_if_fail (TP_IS_CALL_CONTENT_MEDIA_DESCRIPTION (self), NULL);

  return self->priv->object_path;
}

/**
 * tp_call_content_media_description_get_remote_contact:
 * @self: a #TpCallContentMediaDescription
 *
 * <!-- -->
 *
 * Returns: the value of #TpCallContentMediaDescription:remote-contact
 * Since: 0.17.5
 */
TpHandle
tp_call_content_media_description_get_remote_contact (
    TpCallContentMediaDescription *self)
{
  g_return_val_if_fail (TP_IS_CALL_CONTENT_MEDIA_DESCRIPTION (self), 0);

  return self->priv->remote_contact;
}

/**
 * tp_call_content_media_description_add_ssrc:
 * @self: a #TpCallContentMediaDescription
 * @contact: if you use this API, you know what it is about
 * @ssrc: if you use this API, you know what it is about
 *
 * if you use this API, you know what it is about
 *
 * Since: 0.17.5
 */
void
tp_call_content_media_description_add_ssrc (TpCallContentMediaDescription *self,
    TpHandle contact,
    guint ssrc)
{
  GArray *array;
  guint i;

  g_return_if_fail (TP_IS_CALL_CONTENT_MEDIA_DESCRIPTION (self));

  array = g_hash_table_lookup (self->priv->ssrcs,
      GUINT_TO_POINTER (contact));

  if (array == NULL)
    {
      array = g_array_new (FALSE, FALSE, sizeof (guint));
      g_hash_table_insert (self->priv->ssrcs,
          GUINT_TO_POINTER (contact),
          array);
    }

  for (i = 0; i < array->len; i++)
    {
      if (g_array_index (array, guint, i) == ssrc)
        return;
    }
  g_array_append_val (array, ssrc);
}

/**
 * tp_call_content_media_description_append_codec:
 * @self: a #TpCallContentMediaDescription
 * @identifier: if you use this API, you know what it is about
 * @name: if you use this API, you know what it is about
 * @clock_rate: if you use this API, you know what it is about
 * @channels: if you use this API, you know what it is about
 * @updated: if you use this API, you know what it is about
 * @parameters: if you use this API, you know what it is about
 *
 * Add description for a supported codec.
 *
 * Since: 0.17.5
 */
void
tp_call_content_media_description_append_codec (
    TpCallContentMediaDescription *self,
    guint identifier,
    const gchar *name,
    guint clock_rate,
    guint channels,
    gboolean updated,
    GHashTable *parameters)
{
  g_return_if_fail (TP_IS_CALL_CONTENT_MEDIA_DESCRIPTION (self));

  if (parameters == NULL)
    parameters = g_hash_table_new (g_str_hash, g_str_equal);

  g_ptr_array_add (self->priv->codecs, tp_value_array_build (6,
      G_TYPE_UINT, identifier,
      G_TYPE_STRING, name,
      G_TYPE_UINT, clock_rate,
      G_TYPE_UINT, channels,
      G_TYPE_BOOLEAN, updated,
      TP_HASH_TYPE_STRING_STRING_MAP, parameters,
      G_TYPE_INVALID));
}

static void
cancelled_cb (GCancellable *cancellable,
    gpointer user_data)
{
  TpCallContentMediaDescription *self = user_data;

  tp_dbus_daemon_unregister_object (self->priv->dbus_daemon, G_OBJECT (self));

  g_simple_async_result_set_error (self->priv->result,
      G_IO_ERROR, G_IO_ERROR_CANCELLED,
      "Media Description cancelled");
  g_simple_async_result_complete_in_idle (self->priv->result);

  g_clear_object (&self->priv->cancellable);
  g_clear_object (&self->priv->result);
  self->priv->handler_id = 0;
}

void
_tp_call_content_media_description_offer_async (
    TpCallContentMediaDescription *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_return_if_fail (TP_IS_CALL_CONTENT_MEDIA_DESCRIPTION (self));
  g_return_if_fail (self->priv->result == NULL);

  self->priv->result = g_simple_async_result_new (G_OBJECT (self),
    callback, user_data, _tp_call_content_media_description_offer_async);

  if (cancellable != NULL)
    {
      self->priv->cancellable = g_object_ref (cancellable);
      self->priv->handler_id = g_cancellable_connect (
          cancellable, G_CALLBACK (cancelled_cb), self, NULL);
    }

  /* register object on the bus */
  DEBUG ("Registering %s", self->priv->object_path);
  tp_dbus_daemon_register_object (self->priv->dbus_daemon,
      self->priv->object_path, G_OBJECT (self));
}

gboolean
_tp_call_content_media_description_offer_finish (
    TpCallContentMediaDescription *self,
    GAsyncResult *result,
    GHashTable **properties,
    GError **error)
{
  _tp_implement_finish_copy_pointer (self,
      _tp_call_content_media_description_offer_async,
      g_hash_table_ref, properties);
}

GHashTable *
_tp_call_content_media_description_dup_properties (
    TpCallContentMediaDescription *self)
{
  g_return_val_if_fail (TP_IS_CALL_CONTENT_MEDIA_DESCRIPTION (self), NULL);

  return tp_asv_new (
      TP_PROP_CALL_CONTENT_MEDIA_DESCRIPTION_INTERFACES,
          G_TYPE_STRV, tp_call_content_media_description_interfaces,
      TP_PROP_CALL_CONTENT_MEDIA_DESCRIPTION_FURTHER_NEGOTIATION_REQUIRED,
          G_TYPE_BOOLEAN, self->priv->further_negotiation_required,
      TP_PROP_CALL_CONTENT_MEDIA_DESCRIPTION_HAS_REMOTE_INFORMATION,
          G_TYPE_BOOLEAN, self->priv->has_remote_information,
      TP_PROP_CALL_CONTENT_MEDIA_DESCRIPTION_CODECS,
          TP_ARRAY_TYPE_CODEC_LIST, self->priv->codecs,
      TP_PROP_CALL_CONTENT_MEDIA_DESCRIPTION_REMOTE_CONTACT,
          G_TYPE_UINT, self->priv->remote_contact,
      TP_PROP_CALL_CONTENT_MEDIA_DESCRIPTION_SSRCS,
          TP_HASH_TYPE_CONTACT_SSRCS_MAP, self->priv->ssrcs,
      NULL);
}

static void
tp_call_content_media_description_accept (TpSvcCallContentMediaDescription *iface,
    GHashTable *properties,
    DBusGMethodInvocation *context)
{
  TpCallContentMediaDescription *self = (TpCallContentMediaDescription *) iface;
  GPtrArray *codecs;
  gboolean valid;
  TpHandle remote_contact;

  DEBUG ("%s was accepted", self->priv->object_path);

  if (self->priv->cancellable != NULL)
    {
      g_cancellable_disconnect (self->priv->cancellable, self->priv->handler_id);
      g_clear_object (&self->priv->cancellable);
      self->priv->handler_id = 0;
    }

  codecs = tp_asv_get_boxed (properties,
      TP_PROP_CALL_CONTENT_MEDIA_DESCRIPTION_CODECS,
      TP_ARRAY_TYPE_CODEC_LIST);
  if (!codecs || codecs->len == 0)
    {
      GError error = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                       "Codecs can not be empty" };
      dbus_g_method_return_error (context, &error);
      return;
    }

  remote_contact = tp_asv_get_uint32 (properties,
      TP_PROP_CALL_CONTENT_MEDIA_DESCRIPTION_REMOTE_CONTACT,
      &valid);
  if (valid && remote_contact != self->priv->remote_contact)
    {
      GError error = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                       "Remote contact must the same as in request." };
      dbus_g_method_return_error (context, &error);
      return;
    }

  g_simple_async_result_set_op_res_gpointer (self->priv->result,
      g_hash_table_ref (properties), (GDestroyNotify) g_hash_table_unref);
  g_simple_async_result_complete (self->priv->result);
  g_clear_object (&self->priv->result);

  tp_svc_call_content_media_description_return_from_accept (context);

  tp_dbus_daemon_unregister_object (self->priv->dbus_daemon, G_OBJECT (self));
}

static void
tp_call_content_media_description_reject (TpSvcCallContentMediaDescription *iface,
    const GValueArray *reason_array,
    DBusGMethodInvocation *context)
{
  TpCallContentMediaDescription *self = (TpCallContentMediaDescription *) iface;

  DEBUG ("%s was rejected", self->priv->object_path);

  if (!self->priv->has_remote_information)
    {
      GError error = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                       "Can not reject an empty Media Description" };
      dbus_g_method_return_error (context, &error);
      return;
    }

  if (self->priv->cancellable != NULL)
    {
      g_cancellable_disconnect (self->priv->cancellable,
          self->priv->handler_id);
      g_clear_object (&self->priv->cancellable);
      self->priv->handler_id = 0;
    }

  g_simple_async_result_set_error (self->priv->result,
      TP_ERRORS, TP_ERROR_MEDIA_CODECS_INCOMPATIBLE,
      "Media description was rejected");
  g_simple_async_result_complete (self->priv->result);
  g_clear_object (&self->priv->result);

  tp_svc_call_content_media_description_return_from_reject (context);

  tp_dbus_daemon_unregister_object (self->priv->dbus_daemon, G_OBJECT (self));
}

static void
call_content_media_description_iface_init (gpointer iface, gpointer data)
{
  TpSvcCallContentMediaDescriptionClass *klass =
      (TpSvcCallContentMediaDescriptionClass *) iface;

#define IMPLEMENT(x) tp_svc_call_content_media_description_implement_##x (\
    klass, tp_call_content_media_description_##x)
  IMPLEMENT(accept);
  IMPLEMENT(reject);
#undef IMPLEMENT
}
