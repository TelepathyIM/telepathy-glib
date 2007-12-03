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

typedef struct _ExampleEchoFactory ExampleEchoFactory;
typedef struct _ExampleEchoFactoryClass ExampleEchoFactoryClass;
typedef struct _ExampleEchoFactoryPrivate ExampleEchoFactoryPrivate;

struct _ExampleEchoFactoryClass {
    GObjectClass parent_class;
};

struct _ExampleEchoFactory {
    GObject parent;

    ExampleEchoFactoryPrivate *priv;
};

GType example_echo_factory_get_type (void);

/* TYPE MACROS */
#define EXAMPLE_TYPE_ECHO_FACTORY \
  (example_echo_factory_get_type ())
#define EXAMPLE_ECHO_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EXAMPLE_TYPE_ECHO_FACTORY, \
                              ExampleEchoFactory))
#define EXAMPLE_ECHO_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EXAMPLE_TYPE_ECHO_FACTORY, \
                           ExampleEchoFactoryClass))
#define EXAMPLE_IS_ECHO_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EXAMPLE_TYPE_ECHO_FACTORY))
#define EXAMPLE_IS_ECHO_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EXAMPLE_TYPE_ECHO_FACTORY))
#define EXAMPLE_ECHO_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXAMPLE_TYPE_ECHO_FACTORY, \
                              ExampleEchoFactoryClass))

G_END_DECLS

#endif /* #ifndef __EXAMPLE_FACTORY_H__ */
