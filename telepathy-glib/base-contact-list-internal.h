/*<private_header>*/
/* ContactList channel manager - internals
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

#ifndef __TP_BASE_CONTACT_LIST_INTERNAL_H__
#define __TP_BASE_CONTACT_LIST_INTERNAL_H__

#include <telepathy-glib/base-contact-list.h>

G_BEGIN_DECLS

char _tp_base_contact_list_presence_state_to_letter (TpSubscriptionState ps);

G_END_DECLS

#endif
