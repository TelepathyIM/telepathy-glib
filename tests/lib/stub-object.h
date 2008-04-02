#ifndef TESTS_STUB_OBJECT_H
#define TESTS_STUB_OBJECT_H

#include <glib-object.h>

typedef struct { GObject p; } StubObject;
typedef struct { GObjectClass p; } StubObjectClass;

GType stub_object_get_type (void);

#endif
