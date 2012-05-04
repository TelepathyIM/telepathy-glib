/*
 * protocol.h - header for an example Protocol
 * Copyright © 2007-2010 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef EXAMPLE_CHANNELSPECIFIC_PROTOCOL_H
#define EXAMPLE_CHANNELSPECIFIC_PROTOCOL_H

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

typedef struct _ExampleCSHProtocol
    ExampleCSHProtocol;
typedef struct _ExampleCSHProtocolPrivate
    ExampleCSHProtocolPrivate;
typedef struct _ExampleCSHProtocolClass
    ExampleCSHProtocolClass;
typedef struct _ExampleCSHProtocolClassPrivate
    ExampleCSHProtocolClassPrivate;

struct _ExampleCSHProtocolClass {
    TpBaseProtocolClass parent_class;

    ExampleCSHProtocolClassPrivate *priv;
};

struct _ExampleCSHProtocol {
    TpBaseProtocol parent;

    ExampleCSHProtocolPrivate *priv;
};

GType example_csh_protocol_get_type (void);

#define EXAMPLE_TYPE_CSH_PROTOCOL \
    (example_csh_protocol_get_type ())
#define EXAMPLE_CSH_PROTOCOL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        EXAMPLE_TYPE_CSH_PROTOCOL, \
        ExampleCSHProtocol))
#define EXAMPLE_CSH_PROTOCOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
        EXAMPLE_TYPE_CSH_PROTOCOL, \
        ExampleCSHProtocolClass))
#define EXAMPLE_IS_CSH_PROTOCOL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
        EXAMPLE_TYPE_CSH_PROTOCOL))
#define EXAMPLE_IS_CSH_PROTOCOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), \
        EXAMPLE_TYPE_CSH_PROTOCOL))
#define EXAMPLE_CSH_PROTOCOL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        EXAMPLE_TYPE_CSH_PROTOCOL, \
        ExampleCSHProtocolClass))

gboolean example_csh_protocol_check_contact_id (const gchar *id,
    gchar **normal,
    GError **error);

G_END_DECLS

#endif
