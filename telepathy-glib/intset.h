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

#ifndef __TP_INTSET_H__
#define __TP_INTSET_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TP_TYPE_INTSET (tp_intset_get_type ())
GType tp_intset_get_type (void);

typedef struct _TpIntSet TpIntSet;

typedef void (*TpIntFunc) (guint i, gpointer userdata);

TpIntSet *tp_intset_new (void) G_GNUC_WARN_UNUSED_RESULT;
TpIntSet *tp_intset_sized_new (guint size) G_GNUC_WARN_UNUSED_RESULT;
TpIntSet *tp_intset_new_containing (guint element) G_GNUC_WARN_UNUSED_RESULT;
void tp_intset_destroy (TpIntSet *set);
void tp_intset_clear (TpIntSet *set);

void tp_intset_add (TpIntSet *set, guint element);
gboolean tp_intset_remove (TpIntSet *set, guint element);
gboolean tp_intset_is_member (const TpIntSet *set, guint element)
  G_GNUC_WARN_UNUSED_RESULT;

void tp_intset_foreach (const TpIntSet *set, TpIntFunc func,
    gpointer userdata);
GArray *tp_intset_to_array (const TpIntSet *set) G_GNUC_WARN_UNUSED_RESULT;
TpIntSet *tp_intset_from_array (const GArray *array) G_GNUC_WARN_UNUSED_RESULT;

guint tp_intset_size (const TpIntSet *set) G_GNUC_WARN_UNUSED_RESULT;

gboolean tp_intset_is_equal (const TpIntSet *left, const TpIntSet *right)
  G_GNUC_WARN_UNUSED_RESULT;

TpIntSet *tp_intset_copy (const TpIntSet *orig) G_GNUC_WARN_UNUSED_RESULT;
TpIntSet *tp_intset_intersection (const TpIntSet *left, const TpIntSet *right)
  G_GNUC_WARN_UNUSED_RESULT;
TpIntSet *tp_intset_union (const TpIntSet *left, const TpIntSet *right)
  G_GNUC_WARN_UNUSED_RESULT;
TpIntSet *tp_intset_difference (const TpIntSet *left, const TpIntSet *right)
  G_GNUC_WARN_UNUSED_RESULT;
TpIntSet *tp_intset_symmetric_difference (const TpIntSet *left,
    const TpIntSet *right) G_GNUC_WARN_UNUSED_RESULT;

gchar *tp_intset_dump (const TpIntSet *set) G_GNUC_WARN_UNUSED_RESULT;

typedef struct _TpIntSetIter TpIntSetIter;

struct _TpIntSetIter
{
    const TpIntSet *set;
    guint element;
};

#define TP_INTSET_ITER_INIT(set) { (set), (guint)(-1) }

#define tp_intset_iter_init(iter, set) tp_intset_iter_init_inline (iter, set)
static inline void
tp_intset_iter_init_inline (TpIntSetIter *iter, const TpIntSet *set)
{
  g_return_if_fail (iter != NULL);
  iter->set = set;
  iter->element = (guint)(-1);
}

#define tp_intset_iter_reset(iter) tp_intset_iter_reset_inline (iter)
static inline void
tp_intset_iter_reset_inline (TpIntSetIter *iter)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (iter->set != NULL);
  iter->element = (guint)(-1);
}

gboolean tp_intset_iter_next (TpIntSetIter *iter);

G_END_DECLS

#endif /*__TP_INTSET_H__*/
