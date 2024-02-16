#include <gio/gio.h>
#include <spiel.h>

static guint speakers_to_init = 10;

static void
spiel_speaker_new_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
  GMainLoop *loop = user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (SpielSpeaker) speaker = spiel_speaker_new_finish (result, &error);
  if (--speakers_to_init == 0)
    {
      g_main_loop_quit (loop);
    }
}

int
main (int argc, char *argv[])
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

  for (guint i = 0; i < speakers_to_init; i++)
    {
      spiel_speaker_new (NULL, spiel_speaker_new_cb, loop);
    }

  g_main_loop_run (loop);

  return 0;
}