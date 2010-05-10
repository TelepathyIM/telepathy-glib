/* ContactList channel manager
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

#ifndef __TP_CONTACT_LIST_MANAGER_H__
#define __TP_CONTACT_LIST_MANAGER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TpContactListManager TpContactListManager;
typedef struct _TpContactListManagerClass TpContactListManagerClass;
typedef struct _TpContactListManagerPrivate TpContactListManagerPrivate;
typedef struct _TpContactListManagerClassPrivate TpContactListManagerClassPrivate;

struct _TpContactListManagerClass {
    /*<private>*/
    GObjectClass parent_class;
    GCallback _padding[7];
    TpContactListManagerClassPrivate *priv;
};

struct _TpContactListManager {
    /*<private>*/
    GObject parent;
    TpContactListManagerPrivate *priv;
};

GType tp_contact_list_manager_get_type (void);

#define TP_TYPE_CONTACT_LIST_MANAGER \
  (tp_contact_list_manager_get_type ())
#define TP_CONTACT_LIST_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CONTACT_LIST_MANAGER, \
                               TpContactListManager))
#define TP_CONTACT_LIST_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_CONTACT_LIST_MANAGER, \
                            TpContactListManagerClass))
#define TP_IS_CONTACT_LIST_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_CONTACT_LIST_MANAGER))
#define TP_IS_CONTACT_LIST_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_CONTACT_LIST_MANAGER))
#define TP_CONTACT_LIST_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CONTACT_LIST_MANAGER, \
                              TpContactListManagerClass))

G_END_DECLS

#endif
