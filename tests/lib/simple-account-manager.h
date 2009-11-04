/*
 * simple-account-manager.h - header for a simple account manager service.
 *
 * Copyright (C) 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __SIMPLE_ACCOUNT_MANAGER_H__
#define __SIMPLE_ACCOUNT_MANAGER_H__

#include <glib-object.h>
#include <telepathy-glib/dbus-properties-mixin.h>


G_BEGIN_DECLS

typedef struct _SimpleAccountManager SimpleAccountManager;
typedef struct _SimpleAccountManagerClass SimpleAccountManagerClass;
typedef struct _SimpleAccountManagerPrivate SimpleAccountManagerPrivate;

struct _SimpleAccountManagerClass {
    GObjectClass parent_class;
    TpDBusPropertiesMixinClass dbus_props_class;
};

struct _SimpleAccountManager {
    GObject parent;

    SimpleAccountManagerPrivate *priv;
};

GType simple_account_manager_get_type (void);

/* TYPE MACROS */
#define SIMPLE_TYPE_ACCOUNT_MANAGER \
  (simple_account_manager_get_type ())
#define SIMPLE_ACCOUNT_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), SIMPLE_TYPE_ACCOUNT_MANAGER, \
                              SimpleAccountManager))
#define SIMPLE_ACCOUNT_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), SIMPLE_TYPE_ACCOUNT_MANAGER, \
                           SimpleAccountManagerClass))
#define SIMPLE_IS_ACCOUNT_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), SIMPLE_TYPE_ACCOUNT_MANAGER))
#define SIMPLE_IS_ACCOUNT_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), SIMPLE_TYPE_ACCOUNT_MANAGER))
#define SIMPLE_ACCOUNT_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SIMPLE_TYPE_ACCOUNT_MANAGER, \
                              SimpleAccountManagerClass))


G_END_DECLS

#endif /* #ifndef __SIMPLE_ACCOUNT_MANAGER_H__ */
