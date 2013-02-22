/* Tests of TpMessage
 *
 * Copyright © 2013 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "tests/lib/util.h"

static void
test_delivery_report_with_body (void)
{
  TpMessage *message = tp_client_message_new ();
  guint i;
  gchar *text;
  TpChannelTextMessageFlags flags;

  g_test_bug ("61254");

  tp_message_set_uint32 (message, 0, "message-type",
      TP_CHANNEL_TEXT_MESSAGE_TYPE_DELIVERY_REPORT);
  tp_message_set_uint32 (message, 0, "delivery-status",
      TP_DELIVERY_STATUS_PERMANENTLY_FAILED);

  /* message from server (alternative in English) */
  i = tp_message_append_part (message);
  tp_message_set_string (message, i, "alternative", "404");
  tp_message_set_string (message, i, "content-type", "text/plain");
  tp_message_set_string (message, i, "lang", "en");
  tp_message_set_string (message, i, "content",
      "I have no contact with that name");

  /* message from server (alternative in German) */
  i = tp_message_append_part (message);
  tp_message_set_string (message, i, "alternative", "404");
  tp_message_set_string (message, i, "content-type", "text/plain");
  tp_message_set_string (message, i, "lang", "de");
  tp_message_set_string (message, i, "content",
      "Ich habe keinen Kontakt mit diesem Namen");

  text = tp_message_to_text (message, &flags);

  g_assert (text != NULL);
  /* tp_message_to_text should only pick one language, and it's arbitrarily the
   * first. */
  g_assert_cmpstr (text, ==, "I have no contact with that name");

  /* This is a delivery report, so old clients should know that there's
   * something more to the message than just a message.
   */
  g_assert_cmpuint (flags, ==, TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT);

  g_free (text);
  g_object_unref (message);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add_func ("/text-channel/delivery-report-with-body",
      test_delivery_report_with_body);

  return g_test_run ();
}
