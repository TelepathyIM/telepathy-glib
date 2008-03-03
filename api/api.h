#ifndef STREAM_ENGINE_API_H
#define STREAM_ENGINE_API_H

#include <glib-object.h>

#include <telepathy-glib/proxy.h>

#include "api/_gen/enums.h"
#include "api/_gen/cli-misc.h"
#include "api/_gen/svc-misc.h"

G_BEGIN_DECLS

#include "api/_gen/gtypes.h"
#include "api/_gen/interfaces.h"

void stream_engine_cli_init (void);

G_END_DECLS

#endif
