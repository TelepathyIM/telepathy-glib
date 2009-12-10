#ifndef __TPL_OBSERVER_H__
#define __TPL_OBSERVER_H__

#include <glib-object.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <tpl-log-store-empathy.h>

#define TP_IFACE_CHAN_TEXT "org.freedesktop.Telepathy.Channel.Type.Text"

G_BEGIN_DECLS

#define TYPE_TPL_OBSERVER	(tpl_observer_get_type ())
#define TPL_OBSERVER(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TPL_OBSERVER, TplObserver))
#define TPL_OBSERVER_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), TYPE_TPL_OBSERVER, TplObserverClass))
#define IS_TPL_OBSERVER(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TPL_OBSERVER))
#define IS_TPL_OBSERVER_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), TYPE_TPL_OBSERVER))
#define TPL_OBSERVER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_TPL_OBSERVER, TplObserverClass))


#define TPL_OBSERVER_WELL_KNOWN_BUS_NAME \
		"org.freedesktop.Telepathy.Client.HeadlessLogger" 
#define TPL_OBSERVER_OBJECT_PATH \
		"/org/freedesktop/Telepathy/Client/HeadlessLogger"


typedef struct _TplObserver TplObserver;

struct _TplObserver
{
	GObject parent;

	/* private */
	GHashTable *chan_map; // channel_path->tpl_IFACE_channel 
};

typedef struct _TplObserverClass TplObserverClass;
struct _TplObserverClass
{
	GObjectClass parent_class;
	TpDBusPropertiesMixinClass dbus_props_class;
};

GType tpl_observer_get_type (void);
TplObserver *tpl_observer_new (void);

void tpl_headless_logger_init(void);

G_END_DECLS

#endif // __TPL_OBSERVER_H__
