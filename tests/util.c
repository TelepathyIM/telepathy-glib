#include <stdio.h>

#include <glib.h>

#include <telepathy-glib/util.h>

void test_strv_contains (void);

void
test_strv_contains (void)
{
  const gchar * const strv[] = {
      "Pah",
      "Pah",
      "Pah-pah-pah",
      "Patrick!",
      NULL
  };

  g_assert (tp_strv_contains (strv, "Patrick!"));
  g_assert (!tp_strv_contains (strv, "Snakes!"));
}

static void
test_value_array_build (void)
{
  GValueArray *arr;
  const gchar *host = "badger.snakes";
  guint port = 128;

  arr = tp_value_array_build (2,
    G_TYPE_STRING, host,
    G_TYPE_UINT, port,
    G_TYPE_INVALID);

  g_assert (!tp_strdiff (g_value_get_string (g_value_array_get_nth (arr, 0)),
    host));

  g_assert (g_value_get_uint (g_value_array_get_nth (arr, 1)) == port);

  g_value_array_free (arr);
}

int main (int argc, char **argv)
{
  GPtrArray *ptrarray;
  gchar *string;

  g_type_init ();

  g_assert (!tp_strdiff (NULL, NULL));
  g_assert (tp_strdiff ("badger", NULL));
  g_assert (tp_strdiff (NULL, "badger"));
  g_assert (!tp_strdiff ("badger", "badger"));
  g_assert (tp_strdiff ("badger", "mushroom"));

  ptrarray = g_ptr_array_new ();
  g_ptr_array_add (ptrarray, GINT_TO_POINTER (23));
  g_ptr_array_add (ptrarray, GINT_TO_POINTER (42));
  g_assert (tp_g_ptr_array_contains (ptrarray, GINT_TO_POINTER (23)));
  g_assert (tp_g_ptr_array_contains (ptrarray, GINT_TO_POINTER (42)));
  g_assert (!tp_g_ptr_array_contains (ptrarray, GINT_TO_POINTER (666)));
  g_ptr_array_free (ptrarray, TRUE);

  string = tp_escape_as_identifier ("");
  g_assert (!tp_strdiff (string, "_"));
  g_free (string);

  string = tp_escape_as_identifier ("badger");
  g_assert (!tp_strdiff (string, "badger"));
  g_free (string);

  string = tp_escape_as_identifier ("0123abc_xyz\x01\xff");
  g_assert (!tp_strdiff (string, "_30123abc_5fxyz_01_ff"));
  g_free (string);

  test_strv_contains ();

  test_value_array_build ();

  return 0;
}
