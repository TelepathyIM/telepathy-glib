#ifndef __EXAMPLE_EXTENSIONS_H__
#define __EXAMPLE_EXTENSIONS_H__

#include <glib-object.h>
#include <telepathy-glib/connection.h>

#include "examples/extensions/_gen/enums.h"
#include "examples/extensions/_gen/cli-connection.h"
#include "examples/extensions/_gen/svc-connection.h"

G_BEGIN_DECLS

#include "examples/extensions/_gen/gtypes.h"
#include "examples/extensions/_gen/interfaces.h"

void example_cli_init (void);

G_END_DECLS

#endif
