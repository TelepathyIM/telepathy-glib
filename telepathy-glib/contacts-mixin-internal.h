/* TpContactsMixin - internal stuff
 *
 * Copyright © 2008-2010 Collabora Ltd.
 * Copyright © 2008 Nokia Corporation
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

#ifndef __TP_CONTACTS_MIXIN_INTERNAL_H__
#define __TP_CONTACTS_MIXIN_INTERNAL_H__

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/contacts-mixin.h>

G_BEGIN_DECLS

void _tp_contacts_mixin_get_contact_attributes (TpBaseConnection *conn,
  const GArray *handles,
  const char **interfaces,
  gboolean hold,
  DBusGMethodInvocation *context);

G_END_DECLS

#endif
