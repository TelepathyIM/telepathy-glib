/*
 * call-stream.h - high level API for Call streams
 *
 * Copyright (C) 2011 Collabora Ltd. <http://www.collabora.co.uk/>
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
 * SECTION:call-stream
 * @title: TpCallStream
 * @short_description: proxy object for a call stream
 *
 * #TpCallStream is a sub-class of #TpProxy providing convenient API
 * to represent #TpCallChannel's stream.
 */

/**
 * TpCallStream:
 *
 * Data structure representing a #TpCallStream.
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpCallStreamClass:
 *
 * The class of a #TpCallStream.
 *
 * Since: 0.UNRELEASED
 */

#include <config.h>

#include "telepathy-glib/call-stream.h"
#include "telepathy-glib/call-misc.h"
#include "telepathy-glib/errors.h"
#include "telepathy-glib/interfaces.h"
#include "telepathy-glib/proxy-subclass.h"

#define DEBUG_FLAG TP_DEBUG_CALL
#include "telepathy-glib/debug-internal.h"

#include "_gen/tp-cli-call-stream-body.h"

G_DEFINE_TYPE (TpCallStream, tp_call_stream, TP_TYPE_PROXY)

struct _TpCallStreamPrivate
{
  gpointer dummy;
};

static void
tp_call_stream_constructed (GObject *obj)
{
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_call_stream_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (obj);
}

static void
tp_call_stream_class_init (TpCallStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = tp_call_stream_constructed;

  g_type_class_add_private (gobject_class, sizeof (TpCallStreamPrivate));
  tp_call_stream_init_known_interfaces ();
  tp_call_mute_init_known_interfaces ();
}

static void
tp_call_stream_init (TpCallStream *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_CALL_STREAM,
      TpCallStreamPrivate);
}

/**
 * tp_call_stream_init_known_interfaces:
 *
 * Ensure that the known interfaces for #TpCallStream have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_CALL_STREAM.
 *
 * Since: 0.UNRELEASED
 */
void
tp_call_stream_init_known_interfaces (void)
{
  static gsize once = 0;

  if (g_once_init_enter (&once))
    {
      GType tp_type = TP_TYPE_CALL_STREAM;

      tp_proxy_init_known_interfaces ();
      tp_proxy_or_subclass_hook_on_interface_add (tp_type,
          tp_cli_call_stream_add_signals);
      tp_proxy_subclass_add_error_mapping (tp_type,
          TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

      g_once_init_leave (&once, 1);
    }
}
