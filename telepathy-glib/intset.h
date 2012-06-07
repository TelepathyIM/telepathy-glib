/* tp-intset.h - Headers for a Glib-link set of integers
 *
 * Copyright © 2005-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2005-2006 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TP_INTSET_H__
#define __TP_INTSET_H__

#include <glib-object.h>

#include <telepathy-glib/defs.h>

G_BEGIN_DECLS

#define TP_TYPE_INTSET (tp_intset_get_type ())
GType tp_intset_get_type (void);

typedef struct _TpIntset TpIntset;

typedef void (*TpIntFunc) (guint i, gpointer userdata);

TpIntset *tp_intset_new (void) G_GNUC_WARN_UNUSED_RESULT;
TpIntset *tp_intset_sized_new (guint size) G_GNUC_WARN_UNUSED_RESULT;
TpIntset *tp_intset_new_containing (guint element) G_GNUC_WARN_UNUSED_RESULT;
void tp_intset_destroy (TpIntset *set);
void tp_intset_clear (TpIntset *set);

void tp_intset_add (TpIntset *set, guint element);
gboolean tp_intset_remove (TpIntset *set, guint element);
gboolean tp_intset_is_member (const TpIntset *set, guint element)
  G_GNUC_WARN_UNUSED_RESULT;

void tp_intset_foreach (const TpIntset *set, TpIntFunc func,
    gpointer userdata);
GArray *tp_intset_to_array (const TpIntset *set) G_GNUC_WARN_UNUSED_RESULT;
TpIntset *tp_intset_from_array (const GArray *array) G_GNUC_WARN_UNUSED_RESULT;

gboolean tp_intset_is_empty (const TpIntset *set) G_GNUC_WARN_UNUSED_RESULT;
guint tp_intset_size (const TpIntset *set) G_GNUC_WARN_UNUSED_RESULT;

gboolean tp_intset_is_equal (const TpIntset *left, const TpIntset *right)
  G_GNUC_WARN_UNUSED_RESULT;

TpIntset *tp_intset_copy (const TpIntset *orig) G_GNUC_WARN_UNUSED_RESULT;
TpIntset *tp_intset_intersection (const TpIntset *left, const TpIntset *right)
  G_GNUC_WARN_UNUSED_RESULT;
TpIntset *tp_intset_union (const TpIntset *left, const TpIntset *right)
  G_GNUC_WARN_UNUSED_RESULT;
TpIntset *tp_intset_difference (const TpIntset *left, const TpIntset *right)
  G_GNUC_WARN_UNUSED_RESULT;
TpIntset *tp_intset_symmetric_difference (const TpIntset *left,
    const TpIntset *right) G_GNUC_WARN_UNUSED_RESULT;

gchar *tp_intset_dump (const TpIntset *set) G_GNUC_WARN_UNUSED_RESULT;

typedef struct {
    /*<private>*/
    gpointer _dummy[16];
} TpIntsetFastIter;

void tp_intset_fast_iter_init (TpIntsetFastIter *iter,
    const TpIntset *set);

gboolean tp_intset_fast_iter_next (TpIntsetFastIter *iter,
    guint *output);

void tp_intset_union_update (TpIntset *self, const TpIntset *other);
void tp_intset_difference_update (TpIntset *self, const TpIntset *other);

G_END_DECLS

#endif /*__TP_INTSET_H__*/
