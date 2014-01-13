/*
 * call-priv.h
 * Copyright (C) 2012 Collabora Ltd.
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


#ifndef __TF_CALL_PRIV_H__
#define __TF_CALL_PRIV_H__

#include "call-stream.h"

G_BEGIN_DECLS

void _tf_call_content_destroy (TfCallContent *self);
void _tf_call_stream_destroy (TfCallStream *self);

G_END_DECLS

#endif /* __TF_CALL_PRIV_H__  */
