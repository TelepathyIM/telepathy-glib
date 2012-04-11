
#include "config.h"

#include "room-list-chan.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/svc-channel.h>

static void room_list_iface_init (gpointer iface,
    gpointer data);

G_DEFINE_TYPE_WITH_CODE (TpTestsRoomListChan, tp_tests_room_list_chan, TP_TYPE_BASE_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_ROOM_LIST, room_list_iface_init))

enum {
  PROP_SERVER = 1,
  LAST_PROPERTY,
};

/*
enum {
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
*/

struct _TpTestsRoomListChanPriv {
  gchar *server;
};

static void
tp_tests_room_list_chan_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpTestsRoomListChan *self = TP_TESTS_ROOM_LIST_CHAN (object);

  switch (property_id)
    {
      case PROP_SERVER:
        g_value_set_string (value, self->priv->server);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tp_tests_room_list_chan_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpTestsRoomListChan *self = TP_TESTS_ROOM_LIST_CHAN (object);

  switch (property_id)
    {
      case PROP_SERVER:
        g_assert (self->priv->server == NULL); /* construct only */
        self->priv->server = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tp_tests_room_list_chan_constructed (GObject *object)
{
  TpTestsRoomListChan *self = TP_TESTS_ROOM_LIST_CHAN (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) tp_tests_room_list_chan_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  tp_base_channel_register (TP_BASE_CHANNEL (self));
}

static void
tp_tests_room_list_chan_finalize (GObject *object)
{
  TpTestsRoomListChan *self = TP_TESTS_ROOM_LIST_CHAN (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) tp_tests_room_list_chan_parent_class)->finalize;

  g_free (self->priv->server);

  if (chain_up != NULL)
    chain_up (object);
}

static void
fill_immutable_properties (TpBaseChannel *chan,
    GHashTable *properties)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_CLASS (
      tp_tests_room_list_chan_parent_class);

  klass->fill_immutable_properties (chan, properties);

  tp_dbus_properties_mixin_fill_properties_hash (
      G_OBJECT (chan), properties,
      TP_IFACE_CHANNEL_TYPE_ROOM_LIST, "Server",
      NULL);
}

static void
tp_tests_room_list_chan_class_init (
    TpTestsRoomListChanClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  TpBaseChannelClass *base_class = TP_BASE_CHANNEL_CLASS (klass);
  GParamSpec *spec;
  static TpDBusPropertiesMixinPropImpl room_list_props[] = {
      { "Server", "server", NULL, },
      { NULL }
  };

  oclass->get_property = tp_tests_room_list_chan_get_property;
  oclass->set_property = tp_tests_room_list_chan_set_property;
  oclass->constructed = tp_tests_room_list_chan_constructed;
  oclass->finalize = tp_tests_room_list_chan_finalize;

  base_class->channel_type = TP_IFACE_CHANNEL_TYPE_ROOM_LIST;
  base_class->target_handle_type = TP_HANDLE_TYPE_NONE;
  base_class->fill_immutable_properties = fill_immutable_properties;

  spec = g_param_spec_string ("server", "server",
      "Server",
      "",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (oclass, PROP_SERVER, spec);

  tp_dbus_properties_mixin_implement_interface (oclass,
      TP_IFACE_QUARK_CHANNEL_TYPE_ROOM_LIST,
      tp_dbus_properties_mixin_getter_gobject_properties, NULL,
      room_list_props);

  g_type_class_add_private (klass, sizeof (TpTestsRoomListChanPriv));
}

static void
tp_tests_room_list_chan_init (TpTestsRoomListChan *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TESTS_TYPE_ROOM_LIST_CHAN, TpTestsRoomListChanPriv);
}

static void
room_list_iface_init (gpointer iface,
    gpointer data)
{
  //TpSvcChannelTypeRoomListClass *klass = iface;

#define IMPLEMENT(x) \
  tp_svc_channel_type_room_list_implement_##x (klass, room_list_##x)
#undef IMPLEMENT
}
