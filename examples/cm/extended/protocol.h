/*
 * protocol.h - header for an example Protocol
 * Copyright © 2007-2010 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef EXAMPLE_EXTENDED_PROTOCOL_H
#define EXAMPLE_EXTENDED_PROTOCOL_H

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

typedef struct _ExampleExtendedProtocol ExampleExtendedProtocol;
typedef struct _ExampleExtendedProtocolPrivate ExampleExtendedProtocolPrivate;
typedef struct _ExampleExtendedProtocolClass ExampleExtendedProtocolClass;
typedef struct _ExampleExtendedProtocolClassPrivate
    ExampleExtendedProtocolClassPrivate;

struct _ExampleExtendedProtocolClass {
    TpBaseProtocolClass parent_class;

    ExampleExtendedProtocolClassPrivate *priv;
};

struct _ExampleExtendedProtocol {
    TpBaseProtocol parent;

    ExampleExtendedProtocolPrivate *priv;
};

GType example_extended_protocol_get_type (void);

#define EXAMPLE_TYPE_EXTENDED_PROTOCOL \
    (example_extended_protocol_get_type ())
#define EXAMPLE_EXTENDED_PROTOCOL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        EXAMPLE_TYPE_EXTENDED_PROTOCOL, \
        ExampleExtendedProtocol))
#define EXAMPLE_EXTENDED_PROTOCOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
        EXAMPLE_TYPE_EXTENDED_PROTOCOL, \
        ExampleExtendedProtocolClass))
#define EXAMPLE_IS_EXTENDED_PROTOCOL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
        EXAMPLE_TYPE_EXTENDED_PROTOCOL))
#define EXAMPLE_IS_EXTENDED_PROTOCOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), \
        EXAMPLE_TYPE_EXTENDED_PROTOCOL))
#define EXAMPLE_EXTENDED_PROTOCOL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        EXAMPLE_TYPE_EXTENDED_PROTOCOL, \
        ExampleExtendedProtocolClass))

gchar *example_extended_protocol_normalize_contact (const gchar *id,
    GError **error);

G_END_DECLS

#endif
