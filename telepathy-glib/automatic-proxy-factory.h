/*
 * Client channel factory creating TpChannel subclasses when possible
 *
 * Copyright © 2010 Collabora Ltd.
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

#ifndef __TP_AUTOMATIC_CHANNEL_FACTORY_H__
#define __TP_AUTOMATIC_CHANNEL_FACTORY_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TpAutomaticChannelFactory TpAutomaticChannelFactory;
typedef struct _TpAutomaticChannelFactoryClass TpAutomaticChannelFactoryClass;

struct _TpAutomaticChannelFactoryClass {
    /*<public>*/
    GObjectClass parent_class;
};

struct _TpAutomaticChannelFactory {
    /*<private>*/
    GObject parent;
};

GType tp_automatic_channel_factory_get_type (void);

#define TP_TYPE_AUTOMATIC_CHANNEL_FACTORY \
  (tp_automatic_channel_factory_get_type ())
#define TP_AUTOMATIC_CHANNEL_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_AUTOMATIC_CHANNEL_FACTORY, \
                               TpAutomaticChannelFactory))
#define TP_AUTOMATIC_CHANNEL_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_AUTOMATIC_CHANNEL_FACTORY, \
                            TpAutomaticChannelFactoryClass))
#define TP_IS_AUTOMATIC_CHANNEL_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_AUTOMATIC_CHANNEL_FACTORY))
#define TP_IS_AUTOMATIC_CHANNEL_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_AUTOMATIC_CHANNEL_FACTORY))
#define TP_AUTOMATIC_CHANNEL_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_AUTOMATIC_CHANNEL_FACTORY, \
                              TpAutomaticChannelFactoryClass))

TpAutomaticChannelFactory * tp_automatic_channel_factory_new (void);

G_END_DECLS

#endif
