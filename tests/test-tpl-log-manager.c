#include <telepathy-logger/event-text.h>

#define gconf_client_get_bool(obj,key,err) g_print ("%s", key)

#define LOG_ID 0
#define CHAT_ID "echo@test.collabora.co.uk"
#define DIRECTION TPL_ENTRY_DIRECTION_IN

int main (int argc, char **argv)
{
//  TplLogManager *manager;

  g_type_init ();

 // manager = tpl_log_manager_dup_singleton ();



  return 0;
}

