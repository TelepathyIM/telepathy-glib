/*
 * call-content.c - a content in a call.
 *
 * Copyright © 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2009 Nokia Corporation
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

#include "call-content.h"

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/telepathy-glib.h>

#include "call-channel.h"

G_DEFINE_TYPE (ExampleCallContent,
    example_call_content,
    G_TYPE_OBJECT)

enum
{
  PROP_CHANNEL = 1,
  N_PROPS
};

struct _ExampleCallContentPrivate
{
  TpBaseConnection *conn;
  ExampleCallChannel *channel;
};

static void
example_call_content_init (ExampleCallContent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_CALL_CONTENT,
      ExampleCallContentPrivate);
}

static void
constructed (GObject *object)
{
  ExampleCallContent *self = EXAMPLE_CALL_CONTENT (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) example_call_content_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_object_get (self->priv->channel,
      "connection", &self->priv->conn,
      NULL);
}

static void
get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  ExampleCallContent *self = EXAMPLE_CALL_CONTENT (object);

  switch (property_id)
    {
    case PROP_CHANNEL:
      g_value_set_object (value, self->priv->channel);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  ExampleCallContent *self = EXAMPLE_CALL_CONTENT (object);

  switch (property_id)
    {
    case PROP_CHANNEL:
      g_assert (self->priv->channel == NULL);
      self->priv->channel = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
dispose (GObject *object)
{
  ExampleCallContent *self = EXAMPLE_CALL_CONTENT (object);

  if (self->priv->channel != NULL)
    {
      g_object_unref (self->priv->channel);
      self->priv->channel = NULL;
    }

  if (self->priv->conn != NULL)
    {
      g_object_unref (self->priv->conn);
      self->priv->conn = NULL;
    }

  ((GObjectClass *) example_call_content_parent_class)->dispose (object);
}

static void
example_call_content_class_init (ExampleCallContentClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  g_type_class_add_private (klass,
      sizeof (ExampleCallContentPrivate));

  object_class->constructed = constructed;
  object_class->set_property = set_property;
  object_class->get_property = get_property;
  object_class->dispose = dispose;

  param_spec = g_param_spec_object ("channel", "ExampleCallChannel",
      "Media channel that owns this content",
      EXAMPLE_TYPE_CALL_CHANNEL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL, param_spec);
}
