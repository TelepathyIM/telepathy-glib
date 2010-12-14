/*
 * TpTLSCertificate - a TpProxy for TLS certificates
 * Copyright © 2010 Collabora Ltd.
 *
 * Based on EmpathyTLSCertificate:
 * @author Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
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
#include "telepathy-glib/tls-certificate.h"

#include <glib/gstdio.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-internal.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/util-internal.h>

#define DEBUG_FLAG TP_DEBUG_TLS
#include "debug-internal.h"

enum {
  /* proxy properties */
  PROP_CERT_TYPE = 1,
  PROP_CERT_DATA,
  PROP_STATE,
  N_PROPS
};

struct _TpTLSCertificatePrivate {
  /* TLSCertificate properties */
  gchar *cert_type;
  GPtrArray *cert_data;
  TpTLSCertificateState state;
};

G_DEFINE_TYPE (TpTLSCertificate, tp_tls_certificate,
    TP_TYPE_PROXY)

/**
 * TP_TLS_CERTIFICATE_FEATURE_CORE:
 *
 * Expands to a call to a function that returns a quark representing the
 * core functionality of a #TpTLSCertificate.
 *
 * When this feature is prepared, the basic properties of the
 * object have been retrieved and are available for use:
 *
 * <itemizedlist>
 * <listitem>#TpTLSCertificate:cert-type</listitem>
 * <listitem>#TpTLSCertificate:cert-data</listitem>
 * </itemizedlist>
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 */

GQuark
tp_tls_certificate_get_feature_quark_core (void)
{
  return g_quark_from_static_string ("tp-tls-certificate-feature-core");
}

static void
tls_certificate_got_all_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GPtrArray *cert_data;
  TpTLSCertificate *self = TP_TLS_CERTIFICATE (proxy);

  if (error != NULL)
    {
      tp_proxy_invalidate (proxy, error);
      return;
    }

  self->priv->cert_type = g_strdup (tp_asv_get_string (properties,
          "CertificateType"));
  self->priv->state = tp_asv_get_uint32 (properties, "State", NULL);

  cert_data = tp_asv_get_boxed (properties, "CertificateChainData",
      TP_ARRAY_TYPE_UCHAR_ARRAY_LIST);
  g_assert (cert_data != NULL);
  self->priv->cert_data = g_boxed_copy (TP_ARRAY_TYPE_UCHAR_ARRAY_LIST,
      cert_data);

  DEBUG ("Got a certificate chain long %u, of type %s",
      self->priv->cert_data->len, self->priv->cert_type);

  _tp_proxy_set_feature_prepared (proxy, TP_TLS_CERTIFICATE_FEATURE_CORE,
      TRUE);
}

static void
tp_tls_certificate_constructed (GObject *object)
{
  TpTLSCertificate *self = TP_TLS_CERTIFICATE (object);
  void (*constructed) (GObject *) =
    G_OBJECT_CLASS (tp_tls_certificate_parent_class)->constructed;

  if (constructed != NULL)
    constructed (object);

  /* FIXME: if we want change notification for 'state', this is the place */

  tp_cli_dbus_properties_call_get_all (self,
      -1, TP_IFACE_AUTHENTICATION_TLS_CERTIFICATE,
      tls_certificate_got_all_cb, NULL, NULL, NULL);
}

static void
tp_tls_certificate_finalize (GObject *object)
{
  TpTLSCertificate *self = TP_TLS_CERTIFICATE (object);
  TpTLSCertificatePrivate *priv = self->priv;

  DEBUG ("%p", object);

  g_free (priv->cert_type);
  tp_clear_boxed (TP_ARRAY_TYPE_UCHAR_ARRAY_LIST, &priv->cert_data);

  G_OBJECT_CLASS (tp_tls_certificate_parent_class)->finalize (object);
}

static void
tp_tls_certificate_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpTLSCertificate *self = TP_TLS_CERTIFICATE (object);
  TpTLSCertificatePrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_CERT_TYPE:
      g_value_set_string (value, priv->cert_type);
      break;
    case PROP_CERT_DATA:
      g_value_set_boxed (value, priv->cert_data);
      break;
    case PROP_STATE:
      g_value_set_uint (value, priv->state);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_tls_certificate_init (TpTLSCertificate *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_TLS_CERTIFICATE, TpTLSCertificatePrivate);
}

enum {
    FEAT_CORE,
    N_FEAT
};

static const TpProxyFeature *
tp_tls_certificate_list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };

  if (G_LIKELY (features[0].name != 0))
    return features;

  features[FEAT_CORE].name = TP_TLS_CERTIFICATE_FEATURE_CORE;
  features[FEAT_CORE].core = TRUE;

  g_assert (features[N_FEAT].name == 0);
  return features;
}

static void
tp_tls_certificate_class_init (TpTLSCertificateClass *klass)
{
  GParamSpec *pspec;
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  TpProxyClass *pclass = TP_PROXY_CLASS (klass);

  tp_tls_certificate_init_known_interfaces ();

  oclass->get_property = tp_tls_certificate_get_property;
  oclass->constructed = tp_tls_certificate_constructed;
  oclass->finalize = tp_tls_certificate_finalize;

  pclass->interface = TP_IFACE_QUARK_AUTHENTICATION_TLS_CERTIFICATE;
  pclass->must_have_unique_name = TRUE;
  pclass->list_features = tp_tls_certificate_list_features;

  g_type_class_add_private (klass, sizeof (TpTLSCertificatePrivate));

  pspec = g_param_spec_string ("cert-type", "Certificate type",
      "The type of this certificate.",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_CERT_TYPE, pspec);

  pspec = g_param_spec_boxed ("cert-data", "Certificate chain data",
      "The raw DER-encoded certificate chain data.",
      TP_ARRAY_TYPE_UCHAR_ARRAY_LIST,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_CERT_DATA, pspec);

  pspec = g_param_spec_uint ("state", "State",
      "The state of this certificate.",
      TP_TLS_CERTIFICATE_STATE_PENDING, NUM_TP_TLS_CERTIFICATE_STATES -1,
      TP_TLS_CERTIFICATE_STATE_PENDING,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_STATE, pspec);
}

static void
cert_proxy_accept_cb (TpTLSCertificate *self,
    const GError *error,
    gpointer user_data,
    GObject *unused_object G_GNUC_UNUSED)
{
  GSimpleAsyncResult *accept_result = user_data;

  DEBUG ("Callback for accept(), error %p", error);

  if (error != NULL)
    {
      DEBUG ("Error was %s", error->message);
      g_simple_async_result_set_from_error (accept_result, error);
    }

  g_simple_async_result_complete (accept_result);
}

static void
cert_proxy_reject_cb (TpTLSCertificate *self,
    const GError *error,
    gpointer user_data,
    GObject *unused_object G_GNUC_UNUSED)
{
  GSimpleAsyncResult *reject_result = user_data;

  DEBUG ("Callback for reject(), error %p", error);

  if (error != NULL)
    {
      DEBUG ("Error was %s", error->message);
      g_simple_async_result_set_from_error (reject_result, error);
    }

  g_simple_async_result_complete (reject_result);
}

static const gchar *
reject_reason_get_dbus_error (TpTLSCertificateRejectReason reason)
{
  const gchar *retval = NULL;

  switch (reason)
    {
#define EASY_CASE(x) \
    case TP_TLS_CERTIFICATE_REJECT_REASON_ ## x: \
      retval = tp_error_get_dbus_name (TP_ERROR_CERT_ ## x); \
      break
    EASY_CASE (UNTRUSTED);
    EASY_CASE (EXPIRED);
    EASY_CASE (NOT_ACTIVATED);
    EASY_CASE (FINGERPRINT_MISMATCH);
    EASY_CASE (HOSTNAME_MISMATCH);
    EASY_CASE (SELF_SIGNED);
    EASY_CASE (REVOKED);
    EASY_CASE (INSECURE);
    EASY_CASE (LIMIT_EXCEEDED);
#undef EASY_CASE

    case TP_TLS_CERTIFICATE_REJECT_REASON_UNKNOWN:
    default:
      retval = tp_error_get_dbus_name (TP_ERROR_CERT_INVALID);
      break;
    }

  return retval;
}

TpTLSCertificate *
tp_tls_certificate_new (TpDBusDaemon *dbus,
    const gchar *bus_name,
    const gchar *object_path,
    GError **error)
{
  TpTLSCertificate *retval = NULL;

  if (!tp_dbus_check_valid_bus_name (bus_name,
          TP_DBUS_NAME_TYPE_UNIQUE, error))
    goto finally;

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  retval = g_object_new (TP_TYPE_TLS_CERTIFICATE,
      "dbus-daemon", dbus,
      "bus-name", bus_name,
      "object-path", object_path,
      NULL);

finally:
  return retval;
}

void
tp_tls_certificate_accept_async (TpTLSCertificate *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *accept_result;

  g_assert (TP_IS_TLS_CERTIFICATE (self));

  DEBUG ("Accepting TLS certificate");

  accept_result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, tp_tls_certificate_accept_async);

  tp_cli_authentication_tls_certificate_call_accept (self,
      -1, cert_proxy_accept_cb,
      accept_result, g_object_unref, NULL);
}

gboolean
tp_tls_certificate_accept_finish (TpTLSCertificate *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_tls_certificate_accept_async)
}

static GPtrArray *
build_rejections_array (TpTLSCertificateRejectReason reason,
    GHashTable *details)
{
  GPtrArray *retval;
  GValueArray *rejection;

  retval = g_ptr_array_new ();
  rejection = tp_value_array_build (3,
      G_TYPE_UINT, reason,
      G_TYPE_STRING, reject_reason_get_dbus_error (reason),
      TP_HASH_TYPE_STRING_VARIANT_MAP, details,
      NULL);

  g_ptr_array_add (retval, rejection);

  return retval;
}

void
tp_tls_certificate_reject_async (TpTLSCertificate *self,
    TpTLSCertificateRejectReason reason,
    GHashTable *details,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GPtrArray *rejections;
  GSimpleAsyncResult *reject_result;

  g_assert (TP_IS_TLS_CERTIFICATE (self));

  DEBUG ("Rejecting TLS certificate with reason %u", reason);

  rejections = build_rejections_array (reason, details);
  reject_result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, tp_tls_certificate_reject_async);

  tp_cli_authentication_tls_certificate_call_reject (self,
      -1, rejections, cert_proxy_reject_cb,
      reject_result, g_object_unref, NULL);

  tp_clear_boxed (TP_ARRAY_TYPE_TLS_CERTIFICATE_REJECTION_LIST,
      &rejections);
}

gboolean
tp_tls_certificate_reject_finish (TpTLSCertificate *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_tls_certificate_reject_async)
}

#include <telepathy-glib/_gen/tp-cli-tls-cert-body.h>

/**
 * tp_tls_certificate_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpTLSCertificate have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_TLS_CERTIFICATE.
 */
void
tp_tls_certificate_init_known_interfaces (void)
{
  static gsize once = 0;

  if (g_once_init_enter (&once))
    {
      GType tp_type = TP_TYPE_TLS_CERTIFICATE;

      tp_proxy_init_known_interfaces ();
      tp_proxy_or_subclass_hook_on_interface_add (tp_type,
          tp_cli_tls_cert_add_signals);
      tp_proxy_subclass_add_error_mapping (tp_type,
          TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

      g_once_init_leave (&once, 1);
    }
}
