/*
 * factory.h - header for an example channel factory
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __EXAMPLE_FACTORY_H__
#define __EXAMPLE_FACTORY_H__

#include <glib-object.h>
#include <telepathy-glib/channel-factory-iface.h>

G_BEGIN_DECLS

typedef struct _ExampleCSHRoomFactory ExampleCSHRoomFactory;
typedef struct _ExampleCSHRoomFactoryClass ExampleCSHRoomFactoryClass;
typedef struct _ExampleCSHRoomFactoryPrivate ExampleCSHRoomFactoryPrivate;

struct _ExampleCSHRoomFactoryClass {
    GObjectClass parent_class;
};

struct _ExampleCSHRoomFactory {
    GObject parent;

    ExampleCSHRoomFactoryPrivate *priv;
};

GType example_csh_room_factory_get_type (void);

/* TYPE MACROS */
#define EXAMPLE_TYPE_CSH_ROOM_FACTORY \
  (example_csh_room_factory_get_type ())
#define EXAMPLE_CSH_ROOM_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EXAMPLE_TYPE_CSH_ROOM_FACTORY, \
                              ExampleCSHRoomFactory))
#define EXAMPLE_CSH_ROOM_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EXAMPLE_TYPE_CSH_ROOM_FACTORY, \
                           ExampleCSHRoomFactoryClass))
#define EXAMPLE_IS_CSH_ROOM_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EXAMPLE_TYPE_CSH_ROOM_FACTORY))
#define EXAMPLE_IS_CSH_ROOM_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EXAMPLE_TYPE_CSH_ROOM_FACTORY))
#define EXAMPLE_CSH_ROOM_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXAMPLE_TYPE_CSH_ROOM_FACTORY, \
                              ExampleCSHRoomFactoryClass))

G_END_DECLS

#endif /* #ifndef __EXAMPLE_FACTORY_H__ */
