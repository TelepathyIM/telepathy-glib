/*
 * call-content.c - proxy for a Content in a Call channel
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
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

#include "extensions/call-content.h"

#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/telepathy-glib.h>

#include "extensions/extensions.h"

/* Generated code */
#include "_gen/cli-call-content-body.h"

/**
 * SECTION:call-content
 * @title: TfFutureCallContent
 * @short_description: proxy for a Content in a Call channel
 * @see_also: #TpChannel
 *
 * FIXME
 *
 * Since: FIXME
 */

/**
 * TfFutureCallContentClass:
 *
 * The class of a #TfFutureCallContent.
 *
 * Since: FIXME
 */
struct _TfFutureCallContentClass {
    TpProxyClass parent_class;
    /*<private>*/
    gpointer priv;
};

/**
 * TfFutureCallContent:
 *
 * A proxy object for a Telepathy connection manager.
 *
 * Since: FIXME
 */
struct _TfFutureCallContent {
    TpProxy parent;
    /*<private>*/
    TfFutureCallContentPrivate *priv;
};

struct _TfFutureCallContentPrivate {
    int dummy;
};

G_DEFINE_TYPE (TfFutureCallContent,
    tf_future_call_content,
    TP_TYPE_PROXY);

static void
tf_future_call_content_init (TfFutureCallContent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TF_FUTURE_TYPE_CALL_CONTENT,
      TfFutureCallContentPrivate);
}

static void
tf_future_call_content_class_init (TfFutureCallContentClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;

  g_type_class_add_private (klass, sizeof (TfFutureCallContentPrivate));

  proxy_class->must_have_unique_name = TRUE;
  proxy_class->interface = TF_FUTURE_IFACE_QUARK_CALL_CONTENT;
  tf_future_call_content_init_known_interfaces ();
}

/**
 * tf_future_call_content_new:
 * @channel: the Call channel
 * @object_path: the object path of the content; may not be %NULL
 * @error: used to indicate the error if %NULL is returned
 *
 * <!-- -->
 *
 * Returns: a new content proxy, or %NULL on invalid arguments
 *
 * Since: FIXME
 */
TfFutureCallContent *
tf_future_call_content_new (TpChannel *channel,
    const gchar *object_path,
    GError **error)
{
  TfFutureCallContent *ret = NULL;

  g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  ret = TF_FUTURE_CALL_CONTENT (g_object_new (TF_FUTURE_TYPE_CALL_CONTENT,
        /* FIXME: pass in the Channel as a property? */
        "dbus-daemon", tp_proxy_get_dbus_daemon (channel),
        "bus-name", tp_proxy_get_bus_name (channel),
        "object-path", object_path,
        NULL));

finally:
  return ret;
}

/**
 * tf_future_call_content_init_known_interfaces:
 *
 * Ensure that the known interfaces for TfFutureCallContent have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TF_FUTURE_TYPE_CALL_CONTENT.
 *
 * Since: 0.7.32
 */
void
tf_future_call_content_init_known_interfaces (void)
{
  static gsize once = 0;

  if (g_once_init_enter (&once))
    {
      GType tp_type = TF_FUTURE_TYPE_CALL_CONTENT;

      tp_proxy_init_known_interfaces ();
      tp_proxy_or_subclass_hook_on_interface_add (tp_type,
          tf_future_cli_call_content_add_signals);
      tp_proxy_subclass_add_error_mapping (tp_type,
          TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

      g_once_init_leave (&once, 1);
    }
}
