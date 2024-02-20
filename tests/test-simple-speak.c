#include <spiel.h>

static GMainLoop *main_loop;
static SpielSpeaker *speaker = NULL;

static void
speaking_cb (SpielSpeaker *_speaker, GParamSpec *pspec, gpointer user_data)
{
  gboolean speaking = FALSE;
  g_object_get (speaker, "speaking", &speaking, NULL);

  if (!speaking)
    {
      g_main_loop_quit (main_loop);
    }
}

static void
speaker_new_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GError *err = NULL;
  SpielUtterance *utterance = spiel_utterance_new ("hello world");
  gboolean speaking = FALSE;

  speaker = spiel_speaker_new_finish (result, &err);
  g_assert_no_error (err);
  if (err)
    g_error_free (err);

  g_assert (speaker != NULL);
  g_assert (utterance != NULL);

  // prevents mock3 from being used
  spiel_utterance_set_language (utterance, "hy");

  g_object_get (speaker, "speaking", &speaking, NULL);
  g_assert_false (speaking);

  spiel_speaker_speak (speaker, utterance);
  g_object_unref (utterance);

  g_object_get (speaker, "speaking", &speaking, NULL);
  g_assert_true (speaking);

  g_signal_connect (speaker, "notify::speaking", G_CALLBACK (speaking_cb),
                    NULL);
}

static void
test_speak (void)
{
  main_loop = g_main_loop_new (NULL, FALSE);
  spiel_speaker_new (NULL, speaker_new_cb, NULL);
  g_main_loop_run (main_loop);
  g_object_unref (speaker);
}

gint
main (gint argc, gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/spiel/speak", test_speak);
  return g_test_run ();
}