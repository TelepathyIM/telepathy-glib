#include <glib.h>

#include <tpl_observer.h>

static GMainLoop *loop = NULL;

int main(int argc, char *argv[])
{
	g_type_init ();

	tpl_headless_logger_init();

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

	return 0;
}
