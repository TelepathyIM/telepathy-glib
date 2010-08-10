/*
 * protocol.h - header for an example Protocol
 * Copyright © 2007-2010 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef EXAMPLE_CALLABLE_PROTOCOL_H
#define EXAMPLE_CALLABLE_PROTOCOL_H

#include <glib-object.h>
#include <telepathy-glib/base-protocol.h>

G_BEGIN_DECLS

typedef struct _ExampleCallableProtocol
    ExampleCallableProtocol;
typedef struct _ExampleCallableProtocolPrivate
    ExampleCallableProtocolPrivate;
typedef struct _ExampleCallableProtocolClass
    ExampleCallableProtocolClass;
typedef struct _ExampleCallableProtocolClassPrivate
    ExampleCallableProtocolClassPrivate;

struct _ExampleCallableProtocolClass {
    TpBaseProtocolClass parent_class;

    ExampleCallableProtocolClassPrivate *priv;
};

struct _ExampleCallableProtocol {
    TpBaseProtocol parent;

    ExampleCallableProtocolPrivate *priv;
};

GType example_callable_protocol_get_type (void);

#define EXAMPLE_TYPE_CALLABLE_PROTOCOL \
    (example_callable_protocol_get_type ())
#define EXAMPLE_CALLABLE_PROTOCOL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        EXAMPLE_TYPE_CALLABLE_PROTOCOL, \
        ExampleCallableProtocol))
#define EXAMPLE_CALLABLE_PROTOCOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
        EXAMPLE_TYPE_CALLABLE_PROTOCOL, \
        ExampleCallableProtocolClass))
#define EXAMPLE_IS_CALLABLE_PROTOCOL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
        EXAMPLE_TYPE_CALLABLE_PROTOCOL))
#define EXAMPLE_IS_CALLABLE_PROTOCOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), \
        EXAMPLE_TYPE_CALLABLE_PROTOCOL))
#define EXAMPLE_CALLABLE_PROTOCOL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        EXAMPLE_TYPE_CALLABLE_PROTOCOL, \
        ExampleCallableProtocolClass))

gboolean example_callable_protocol_check_contact_id (const gchar *id,
    gchar **normal,
    GError **error);

G_END_DECLS

#endif
