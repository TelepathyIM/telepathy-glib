/*
 * simple-account.h - header for a simple account service.
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __SIMPLE_ACCOUNT_H__
#define __SIMPLE_ACCOUNT_H__

#include <glib-object.h>
#include <telepathy-glib/dbus-properties-mixin.h>


G_BEGIN_DECLS

typedef struct _SimpleAccount SimpleAccount;
typedef struct _SimpleAccountClass SimpleAccountClass;
typedef struct _SimpleAccountPrivate SimpleAccountPrivate;

struct _SimpleAccountClass {
    GObjectClass parent_class;
    TpDBusPropertiesMixinClass dbus_props_class;
};

struct _SimpleAccount {
    GObject parent;

    SimpleAccountPrivate *priv;
};

GType simple_account_get_type (void);

/* TYPE MACROS */
#define SIMPLE_TYPE_ACCOUNT \
  (simple_account_get_type ())
#define SIMPLE_ACCOUNT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), SIMPLE_TYPE_ACCOUNT, \
                              SimpleAccount))
#define SIMPLE_ACCOUNT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), SIMPLE_TYPE_ACCOUNT, \
                           SimpleAccountClass))
#define SIMPLE_IS_ACCOUNT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), SIMPLE_TYPE_ACCOUNT))
#define SIMPLE_IS_ACCOUNT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), SIMPLE_TYPE_ACCOUNT))
#define SIMPLE_ACCOUNT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SIMPLE_TYPE_ACCOUNT, \
                              SimpleAccountClass))

G_END_DECLS

#endif /* #ifndef __SIMPLE_ACCOUNT_H__ */
