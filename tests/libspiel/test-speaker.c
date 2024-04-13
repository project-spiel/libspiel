// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright © 2024 GNOME Foundation Inc.
// SPDX-FileContributor: Andy Holmes <andyholmes@gnome.org>

#include <gio/gio.h>
#include <gst/audio/audio.h>
#include <gst/gst.h>
#include <spiel.h>

static void
spiel_speaker_new_cb (GObject *source_object,
                      GAsyncResult *result,
                      SpielSpeaker **speaker_out)
{
  GError *error = NULL;

  *speaker_out = spiel_speaker_new_finish (result, &error);
  g_assert_no_error (error);
}

static void
test_speaker_new (void)
{
  g_autoptr (SpielSpeaker) speaker = NULL;

  spiel_speaker_new (NULL, (GAsyncReadyCallback) spiel_speaker_new_cb,
                     &speaker);

  while (speaker == NULL)
    g_main_context_iteration (NULL, FALSE);

  g_assert_true (SPIEL_IS_SPEAKER (speaker));
}

static void
test_speaker_properties (void)
{
  g_autoptr (SpielSpeaker) speaker = NULL;
  GError *error = NULL;
  gboolean paused_out = FALSE;
  g_autoptr (GListModel) providers_out = NULL;
  g_autoptr (GstElement) sink_out = NULL;
  gboolean speaking_out = FALSE;
  GListModel *voices_out = NULL;

  speaker = spiel_speaker_new_sync (NULL, &error);
  g_assert_no_error (error);
  g_assert_true (SPIEL_IS_SPEAKER (speaker));

  g_object_get (speaker, "paused", &paused_out, "providers", &providers_out,
                "sink", &sink_out, "speaking", &speaking_out, "voices",
                &voices_out, NULL);
  g_assert_cmpuint (paused_out, ==, FALSE);
  g_assert_true (G_IS_LIST_MODEL (providers_out));
  g_assert_true (GST_IS_ELEMENT (sink_out));
  g_assert_cmpuint (speaking_out, ==, FALSE);
  g_assert_true (G_IS_LIST_MODEL (voices_out));

  g_assert_true (G_IS_LIST_MODEL (spiel_speaker_get_providers (speaker)));
  g_assert_true (G_IS_LIST_MODEL (spiel_speaker_get_voices (speaker)));
}

static void
test_speaker_providers (void)
{
  g_autoptr (SpielSpeaker) speaker = NULL;
  g_autoptr (SpielProvider) provider = NULL;
  GError *error = NULL;
  GListModel *providers = NULL;
  GListModel *voices = NULL;

  speaker = spiel_speaker_new_sync (NULL, &error);
  g_assert_no_error (error);
  g_assert_true (SPIEL_IS_SPEAKER (speaker));

  providers = spiel_speaker_get_providers (speaker);
  g_assert_true (G_IS_LIST_MODEL (providers));
  g_assert_cmpuint (g_list_model_get_n_items (providers), ==, 1);

  provider = g_list_model_get_item (providers, 0);
  g_assert_cmpstr (spiel_provider_get_name (provider), ==, "Mock Provider");
  g_assert_cmpstr (spiel_provider_get_well_known_name (provider), ==,
                   "org.mock.Speech.Provider");

  voices = spiel_speaker_get_voices (speaker);
  g_assert_true (G_IS_LIST_MODEL (voices));
  g_assert_cmpuint (g_list_model_get_n_items (voices), ==, 3);
}

static void
on_speaker_changed (SpielSpeaker *speaker, GParamSpec *pspec, gboolean *ret)
{
  if (ret != NULL)
    g_object_get (speaker, pspec->name, ret, NULL);
}

static void
test_speaker_synthesize (void)
{
  g_autoptr (SpielSpeaker) speaker = NULL;
  gboolean speaking = FALSE;
  gboolean paused = FALSE;
  GError *error = NULL;

  speaker = spiel_speaker_new_sync (NULL, &error);
  g_assert_no_error (error);
  g_assert_true (SPIEL_IS_SPEAKER (speaker));

  g_signal_connect (speaker, "notify::speaking",
                    G_CALLBACK (on_speaker_changed), &speaking);
  g_signal_connect (speaker, "notify::paused", G_CALLBACK (on_speaker_changed),
                    &paused);

  /* Speaker is quiescent */
  g_object_get (speaker, "paused", &paused, "speaking", &speaking, NULL);
  g_assert_false (speaking);
  g_assert_false (paused);

  /* Speaker can start speech */
  for (size_t i = 0; i < 5; i++)
    {
      g_autoptr (SpielUtterance) utterance = NULL;

      utterance = spiel_utterance_new ("I was made to understand there were"
                                       " grilled cheese sandwiches here.");
      spiel_speaker_speak (speaker, utterance);
    }
  g_assert_true (speaking);
  g_assert_false (paused);

  /* Speaker can pause speech */
  spiel_speaker_pause (speaker);
  while (!paused)
    g_main_context_iteration (NULL, FALSE);

  g_assert_true (speaking);
  g_assert_true (paused);

  /* Speaker can resume speech */
  spiel_speaker_resume (speaker);
  while (paused)
    g_main_context_iteration (NULL, FALSE);

  g_assert_true (speaking);
  g_assert_false (paused);

  /* Speaker can cancel speech */
  spiel_speaker_cancel (speaker);
  g_assert_false (speaking);
  g_assert_false (paused);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libspiel/speaker/new", test_speaker_new);
  g_test_add_func ("/libspiel/speaker/properties", test_speaker_properties);
  g_test_add_func ("/libspiel/speaker/providers", test_speaker_providers);
  g_test_add_func ("/libspiel/speaker/synthesize", test_speaker_synthesize);

  return g_test_run ();
}
