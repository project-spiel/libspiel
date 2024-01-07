#include <spiel.h>

static GMainLoop *main_loop;
static uint speaking_change_count = 0;

static void
speaking_cb (SpielSpeaker *speaker, GParamSpec *pspec, gpointer user_data)
{
  gboolean speaking = FALSE;
  speaking_change_count++;
  g_object_get(speaker, "speaking", &speaking, NULL);

  if (!speaking) {
    g_object_unref (speaker);
    g_assert_cmpuint(speaking_change_count, ==, 2);
    g_main_loop_quit (main_loop);
  }
}

static void
speaker_new_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GError *err = NULL;
  SpielSpeaker *speaker = spiel_speaker_new_finish(result, &err);
  SpielUtterance *utterance = spiel_utterance_new("hello world");
  g_assert_no_error(err);
  if (err) g_error_free(err);

  g_assert(speaker != NULL);
  g_assert(utterance != NULL);

  // prevents mock3 from being used
  spiel_utterance_set_language(utterance, "en");

  spiel_speaker_speak(speaker, utterance);
  g_object_unref (utterance);

  g_signal_connect(speaker, "notify::speaking", G_CALLBACK(speaking_cb), NULL);
}

static void
test_speak (void)
{
  main_loop = g_main_loop_new (NULL, FALSE);
  spiel_speaker_new(NULL, speaker_new_cb, NULL);
  g_main_loop_run (main_loop);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/spiel/speak", test_speak);
  return g_test_run ();
}