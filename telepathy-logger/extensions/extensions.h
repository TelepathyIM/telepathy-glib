#ifndef _TPL_EXTENSIONS_H
#define _TPL_EXTENSIONS_H

#include <telepathy-glib/telepathy-glib.h>

#include "telepathy-logger/extensions/_gen/enums.h"
#include "telepathy-logger/extensions/_gen/cli-misc.h"
#include "telepathy-logger/extensions/_gen/svc-misc.h"

G_BEGIN_DECLS

#include "telepathy-logger/extensions/_gen/gtypes.h"
#include "telepathy-logger/extensions/_gen/interfaces.h"

G_END_DECLS

void tpl_cli_init (void);

#endif /* _TPL_EXTENSIONS_H */

