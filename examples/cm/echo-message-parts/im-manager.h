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

#ifndef EXAMPLE_ECHO_MESSAGE_PARTS_FACTORY_H
#define EXAMPLE_ECHO_MESSAGE_PARTS_FACTORY_H

#include <glib-object.h>
#include <telepathy-glib/channel-factory-iface.h>

G_BEGIN_DECLS

typedef struct _ExampleEcho2Factory ExampleEcho2Factory;
typedef struct _ExampleEcho2FactoryClass ExampleEcho2FactoryClass;
typedef struct _ExampleEcho2FactoryPrivate ExampleEcho2FactoryPrivate;

struct _ExampleEcho2FactoryClass {
    GObjectClass parent_class;
};

struct _ExampleEcho2Factory {
    GObject parent;

    ExampleEcho2FactoryPrivate *priv;
};

GType example_echo_2_factory_get_type (void);

/* TYPE MACROS */
#define EXAMPLE_TYPE_ECHO_2_FACTORY \
  (example_echo_2_factory_get_type ())
#define EXAMPLE_ECHO_2_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EXAMPLE_TYPE_ECHO_2_FACTORY, \
                              ExampleEcho2Factory))
#define EXAMPLE_ECHO_2_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EXAMPLE_TYPE_ECHO_2_FACTORY, \
                           ExampleEcho2FactoryClass))
#define EXAMPLE_IS_ECHO_2_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EXAMPLE_TYPE_ECHO_2_FACTORY))
#define EXAMPLE_IS_ECHO_2_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EXAMPLE_TYPE_ECHO_2_FACTORY))
#define EXAMPLE_ECHO_2_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXAMPLE_TYPE_ECHO_2_FACTORY, \
                              ExampleEcho2FactoryClass))

G_END_DECLS

#endif
