/*
 * cli-misc.c - host the generated code for most Telepathy classes
 *
 * Copyright © 2007-2012 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2011 Nokia Corporation
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

#include "config.h"

#include <telepathy-glib/cli-call.h>
#include <telepathy-glib/cli-misc.h>

#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#include "telepathy-glib/_gen/tp-cli-account-body.h"
#include "telepathy-glib/_gen/tp-cli-account-manager-body.h"
#include "telepathy-glib/_gen/tp-cli-call-content-body.h"
#include "telepathy-glib/_gen/tp-cli-call-content-media-description-body.h"
#include "telepathy-glib/_gen/tp-cli-call-stream-body.h"
#include "telepathy-glib/_gen/tp-cli-call-stream-endpoint-body.h"
#include "telepathy-glib/_gen/tp-cli-channel-dispatcher-body.h"
#include "telepathy-glib/_gen/tp-cli-channel-dispatch-operation-body.h"
#include "telepathy-glib/_gen/tp-cli-channel-request-body.h"
#include "telepathy-glib/_gen/tp-cli-client-body.h"
#include "telepathy-glib/_gen/tp-cli-connection-manager-body.h"
#include "telepathy-glib/_gen/tp-cli-dbus-daemon-body.h"
#include "telepathy-glib/_gen/tp-cli-generic-body.h"
#include "telepathy-glib/_gen/tp-cli-protocol-body.h"
