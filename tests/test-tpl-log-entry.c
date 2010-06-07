#include <telepathy-logger/entry-text.h>
#include <telepathy-logger/entry-text-internal.h>

#define gconf_client_get_bool(obj,key,err) g_print ("%s", key)

#define LOG_ID "0"
#define ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/FOO/BAR/BAZ"
#define CHAT_ID "echo@test.collabora.co.uk"
#define DIRECTION TPL_LOG_ENTRY_DIRECTION_IN

int main (int argc, char **argv)
{
  TplLogEntryText *log;

  g_type_init ();

  log = _tpl_log_entry_text_new (LOG_ID, ACCOUNT_PATH, DIRECTION);


  return 0;
}

