/*
 * telepathy-farstream.c - Global functions for telepathy-farstream
 * Copyright (C) 2011 Collabora Ltd.
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

#include "extensions/extensions.h"
#include "telepathy-farstream.h"

/**
 * tf_init:
 *
 * Initializes telepathy-farstream. This must be called at the start of your
 * application, specifically before any DBus proxies related to Call channels
 * are created
 */
void
tf_init (void)
{
  tf_future_cli_init ();
}
