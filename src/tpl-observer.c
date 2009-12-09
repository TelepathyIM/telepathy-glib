#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/svc-client.h>

#include <tpl-observer.h>
#include <tpl-channel.h>
#include <tpl-text-channel-context.h>

static void observer_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (TplObserver, tpl_observer, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
		tp_dbus_properties_mixin_iface_init);
	G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CLIENT,
		NULL);
	G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CLIENT_OBSERVER,
		observer_iface_init);
);

static const char *client_interfaces[] = {
    TP_IFACE_CLIENT_OBSERVER,
    NULL
};

enum
{
  PROP_0,
  PROP_INTERFACES,
  PROP_CHANNEL_FILTER
};

static void _observe_channel_when_ready_cb(TpChannel *channel,
		const GError *error,
		gpointer user_data)
{
	TplChannel *tpl_chan = TPL_CHANNEL(user_data);
	gpointer tpl_channel_instance;
	// TODO add a GHashTable mapping
	// channelpath:tpl_channel_instance to record observed channels
	// and a way to access (get/set) them

	if(error!=NULL) {
		g_error("channel error: %s", error->message);
		g_error("giving up observing channel '%s'",
				tpl_chan->channel_path);
		// TODO free tpl_chan
		//tpl_channel_data_free(tpl_chan);
		return;
	}

	tpl_channel_set_channel_type(tpl_chan, 
			tp_channel_get_channel_type(tpl_chan->channel) );

	/* Instantiate and delegate channel handling to the right object */
	if(0==g_strcmp0(TP_IFACE_CHAN_TEXT, tpl_chan->channel_type)) {
		tpl_channel_instance = tpl_text_channel_new(tpl_chan);	
		// TODO add to hash
	} else {
		g_warning("%s: channel not handled by TPL", tpl_chan->channel_type);
	}
}


static void _tp_connection_called_when_ready_cb(TpConnection *connection,
		const GError *error,
		gpointer user_data)
{
	TplChannel *tpl_chan = TPL_CHANNEL(user_data);

	tp_channel_call_when_ready (tpl_channel_get_channel(tpl_chan),
			_observe_channel_when_ready_cb, tpl_chan);
}


static void
tpl_observer_observe_channels (TpSvcClientObserver   *self,
                                   const char            *account,
                                   const char            *connection,
                                   const GPtrArray       *channels,
                                   const char            *dispatch_op,
                                   const GPtrArray       *requests_satisfied,
                                   GHashTable            *observer_info,
                                   DBusGMethodInvocation *context)
{
  TpAccount *tp_acc;
  TpConnection *tp_conn;
  TpDBusDaemon *tp_bus_daemon;
  GError *error = NULL;

  g_debug (" > tpl_observer_observe_channels\n");
  g_debug ("     account    = %s\n", account);
  g_debug ("     connection = %s\n", connection);
  g_debug ("     dispatchop = %s\n", dispatch_op);
 
  tp_bus_daemon = tp_dbus_daemon_dup(&error);
  if(tp_bus_daemon == NULL) {
	g_error("%s\n", error->message);   
	g_clear_error(&error);
	g_error_free(error);
  }

  tp_acc = tp_account_new(tp_bus_daemon, account, &error);
  if(tp_acc == NULL) {
	  g_error("%s\n", error->message);
	  g_clear_error(&error);
	  g_error_free(error);
	  error=NULL;
  }


  tp_conn = tp_connection_new (tp_bus_daemon, NULL, connection, &error);
  if(tp_conn == NULL) {
	  g_error("%s\n", error->message);   
	  g_clear_error(&error);
	  g_error_free(error);
	  error=NULL;
  }

  /* channels is of type a(oa{sv}) */
  for (guint i = 0; i < channels->len; i++)
    {
      GValueArray *channel = g_ptr_array_index (channels, i);
      TpChannel *tp_chan = NULL;
      TplChannel* tpl_chan = tpl_channel_new(self);

      char *path = g_value_get_boxed (g_value_array_get_nth (channel, 0));
      // propertyNameStr/value hash
      GHashTable *map = g_value_get_boxed (g_value_array_get_nth (channel, 1));

      g_debug ("     channel    = %s\n", path);
      tp_chan = tp_channel_new (tp_conn, path, NULL, TP_UNKNOWN_HANDLE_TYPE, 0, 
		&error);

      tpl_channel_set_account(tpl_chan, tp_acc);
      tpl_channel_set_account_path(tpl_chan, account);
      tpl_channel_set_connection(tpl_chan, tp_conn);
      tpl_channel_set_connection_path(tpl_chan, connection);
      tpl_channel_set_channel(tpl_chan, tp_chan);
      tpl_channel_set_channel_path(tpl_chan, path);
      tpl_channel_set_channel_properties(tpl_chan, map);

      tp_connection_call_when_ready(tp_conn,
			_tp_connection_called_when_ready_cb, tpl_chan);
    }

  

  tp_svc_client_observer_return_from_observe_channels (context);
}

static void
tpl_observer_get_property (GObject    *self,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  g_print(" :: get_property\n");
  switch (property_id)
    {
	GPtrArray *array;
	GHashTable *map;
      case PROP_INTERFACES:
        g_print (" :: interfaces\n");
        g_value_set_boxed (value, client_interfaces);
        break;

      case PROP_CHANNEL_FILTER:
        g_print (" :: channel-filter\n");

        /* create an empty filter - which means all channels */
        array = g_ptr_array_new ();
        map = g_hash_table_new (NULL, NULL);

        g_ptr_array_add (array, map);
        g_value_set_boxed (value, array);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
        break;
    }
}

static void
tpl_observer_class_init (TplObserverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* D-Bus properties are exposed as GObject properties through the
   * TpDBusPropertiesMixin */
  /* properties on the Client interface */
  static TpDBusPropertiesMixinPropImpl client_props[] = {
        { "Interfaces", "interfaces", NULL },
        { NULL }
  };

  /* properties on the Client.Observer interface */
  static TpDBusPropertiesMixinPropImpl client_observer_props[] = {
        { "ObserverChannelFilter", "channel-filter", NULL },
        { NULL }
  };

  /* complete list of interfaces with properties */
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
        { TP_IFACE_CLIENT,
          tp_dbus_properties_mixin_getter_gobject_properties,
          NULL,
          client_props
        },
        { TP_IFACE_CLIENT_OBSERVER,
          tp_dbus_properties_mixin_getter_gobject_properties,
          NULL,
          client_observer_props
        },
        { NULL }
  };
  object_class->get_property = tpl_observer_get_property;

  g_object_class_install_property (object_class, PROP_INTERFACES,
      g_param_spec_boxed ("interfaces",
                          "Interfaces",
                          "Available D-Bus Interfaces",
                          G_TYPE_STRV,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_CHANNEL_FILTER,
      g_param_spec_boxed ("channel-filter",
                          "Channel Filter",
                          "Filter for channels we observe",
                          TP_ARRAY_TYPE_CHANNEL_CLASS_LIST,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /* call our mixin class init */
  klass->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TplObserverClass, dbus_props_class));
}

static void
tpl_observer_init (TplObserver *self)
{
}

static void
observer_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcClientObserverClass *klass = (TpSvcClientObserverClass *) g_iface;

#define IMPLEMENT(x) tp_svc_client_observer_implement_##x (klass, \
    tpl_observer_##x)
  IMPLEMENT (observe_channels);
#undef IMPLEMENT
}

TplObserver *tpl_observer_new (void)
{
  return g_object_new (TYPE_TPL_OBSERVER, NULL);
}
