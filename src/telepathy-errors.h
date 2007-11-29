/*
 * telepathy-errors.h - Header for Telepathy error types
 * Copyright (C) 2005 Collabora Ltd.
 * Copyright (C) 2005 Nokia Corporation
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

#ifndef __TELEPATHY_ERRORS_H__
#define __TELEPATHY_ERRORS_H__

#include <telepathy-glib/errors.h>

#define InvalidHandle TP_ERROR_INVALID_HANDLE
#define Disconnected TP_ERROR_DISCONNECTED
#define InvalidArgument TP_ERROR_INVALID_ARGUMENT
#define NetworkError TP_ERROR_NETWORK_ERROR
#define PermissionDenied TP_ERROR_PERMISSION_DENIED
#define NotAvailable TP_ERROR_NOT_AVAILABLE
#define NotImplemented TP_ERROR_NOT_IMPLEMENTED
#define TelepathyErrors TpError
#define TELEPATHY_ERRORS TP_ERRORS
#define TELEPATHY_TYPE_ERRORS TP_TYPE_ERROR

#endif /* #ifndef __TELEPATHY_ERRORS_H__*/
