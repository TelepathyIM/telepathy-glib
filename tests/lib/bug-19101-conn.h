/*
 * bug-19101-conn.h - header for a broken connection to reproduce bug #19101
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef TESTS_LIB_BUG_19101_CONN_H
#define TESTS_LIB_BUG_19101_CONN_H

#include "contacts-conn.h"

G_BEGIN_DECLS

typedef struct _Bug19101Connection Bug19101Connection;
typedef struct _Bug19101ConnectionClass Bug19101ConnectionClass;

struct _Bug19101ConnectionClass {
    ContactsConnectionClass parent_class;
};

struct _Bug19101Connection {
    ContactsConnection parent;
};

GType bug_19101_connection_get_type (void);

/* TYPE MACROS */
#define BUG_19101_TYPE_CONNECTION \
  (bug_19101_connection_get_type ())
#define BUG_19101_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), BUG_19101_TYPE_CONNECTION, \
                              Bug19101Connection))
#define BUG_19101_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), BUG_19101_TYPE_CONNECTION, \
                           Bug19101ConnectionClass))
#define BUG_19101_IS_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), BUG_19101_TYPE_CONNECTION))
#define BUG_19101_IS_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), BUG_19101_TYPE_CONNECTION))
#define BUG_19101_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), BUG_19101_TYPE_CONNECTION, \
                              Bug19101ConnectionClass))

G_END_DECLS

#endif
