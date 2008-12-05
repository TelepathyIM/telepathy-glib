#include <pygobject.h>

#include <gst/gst.h>

void tf_register_classes (PyObject *d);

DL_EXPORT(void) inittpfarsight(void);
extern PyMethodDef tf_functions[];

GST_DEBUG_CATEGORY (pygst_debug); /* for python code */

DL_EXPORT(void)
inittpfarsight(void)
{
  PyObject *m, *d;

  init_pygobject ();

  m = Py_InitModule ("tpfarsight", tf_functions);
  d = PyModule_GetDict (m);

  tf_register_classes (d);

  if (PyErr_Occurred ()) {
    PyErr_Print();
    Py_FatalError ("can't initialise module tpfarsight");
  }
}
