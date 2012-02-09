/*
 * echo-channel-manager-conn.c
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include "echo-channel-manager-conn.h"
#include "simple-channel-manager.h"

G_DEFINE_TYPE (TpTestsEchoChannelManagerConnection,
    tp_tests_echo_channel_manager_connection,
    TP_TESTS_TYPE_ECHO_CONNECTION)

/* type definition stuff */

enum
{
  PROP_CHANNEL_MANAGER = 1,
  N_PROPS
};

struct _TpTestsEchoChannelManagerConnectionPrivate
{
  TpTestsSimpleChannelManager *channel_manager;
};

static void
tp_tests_echo_channel_manager_connection_init (TpTestsEchoChannelManagerConnection *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TESTS_TYPE_ECHO_CHANNEL_MANAGER_CONNECTION,
      TpTestsEchoChannelManagerConnectionPrivate);
}

static void
get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *spec)
{
  TpTestsEchoChannelManagerConnection *self = TP_TESTS_ECHO_CHANNEL_MANAGER_CONNECTION (object);

  switch (property_id) {
    case PROP_CHANNEL_MANAGER:
      g_value_set_object (value, self->priv->channel_manager);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
  }
}

static void
set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *spec)
{
  TpTestsEchoChannelManagerConnection *self = TP_TESTS_ECHO_CHANNEL_MANAGER_CONNECTION (object);

  switch (property_id) {
    case PROP_CHANNEL_MANAGER:
      self->priv->channel_manager = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
  }
}

static GPtrArray *
create_channel_managers (TpBaseConnection *conn)
{
  TpTestsEchoChannelManagerConnection *self = TP_TESTS_ECHO_CHANNEL_MANAGER_CONNECTION (conn);
  GPtrArray *ret = g_ptr_array_sized_new (1);

  /* tp-glib will free this for us so we don't need to worry about
     doing it ourselves. */
  g_ptr_array_add (ret, self->priv->channel_manager);

  return ret;
}

static void
tp_tests_echo_channel_manager_connection_class_init (TpTestsEchoChannelManagerConnectionClass *klass)
{
  TpBaseConnectionClass *base_class =
      (TpBaseConnectionClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  g_type_class_add_private (klass, sizeof (TpTestsEchoChannelManagerConnectionPrivate));

  base_class->create_channel_managers = create_channel_managers;

  param_spec = g_param_spec_object ("channel-manager", "Channel manager",
      "The channel manager", TP_TESTS_TYPE_SIMPLE_CHANNEL_MANAGER,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_MANAGER, param_spec);
}
