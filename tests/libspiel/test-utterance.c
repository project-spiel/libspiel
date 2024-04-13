// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright © 2024 GNOME Foundation Inc.
// SPDX-FileContributor: Andy Holmes <andyholmes@gnome.org>

#include <gio/gio.h>

#include <speech-provider.h>
#include <spiel.h>

static void
test_utterance_new (void)
{
  SpielUtterance *utterance = NULL;

  utterance = spiel_utterance_new (NULL);
  g_object_unref (utterance);
}

static void
test_utterance_properties (void)
{
  g_autoptr (SpielUtterance) utterance = NULL;
  gboolean is_ssml = FALSE;
  const char *language = "en-US";
  double pitch = g_random_double_range (0.0, 2.0);
  double rate = g_random_double_range (0.1, 10.0);
  g_autofree char *text = g_uuid_string_random ();
  g_autoptr (SpielVoice) voice = NULL;
  double volume = 1.0;

  gboolean is_ssml_out = FALSE;
  g_autofree char *language_out = NULL;
  double pitch_out = 1.0;
  double rate_out = 1.0;
  g_autofree char *text_out = NULL;
  g_autoptr (SpielVoice) voice_out = NULL;
  double volume_out = 1.0;

  utterance =
      g_object_new (SPIEL_TYPE_UTTERANCE, "is-ssml", is_ssml, "language",
                    language, "pitch", pitch, "rate", rate, "text", text,
                    "voice", voice, "volume", volume, NULL);

  g_object_get (utterance, "is-ssml", &is_ssml_out, "language", &language_out,
                "pitch", &pitch_out, "rate", &rate_out, "text", &text_out,
                "voice", &voice_out, "volume", &volume_out, NULL);
  g_assert_true (is_ssml == is_ssml_out);
  g_assert_cmpstr (language, ==, language_out);
  g_assert_cmpfloat_with_epsilon (pitch, pitch_out, DBL_EPSILON);
  g_assert_cmpfloat_with_epsilon (rate, rate_out, DBL_EPSILON);
  g_assert_cmpstr (text, ==, text_out);
  g_assert_true (voice == voice_out);
  g_assert_cmpfloat_with_epsilon (volume, volume_out, DBL_EPSILON);

  g_assert_true (is_ssml == spiel_utterance_get_is_ssml (utterance));
  g_assert_cmpstr (language, ==, spiel_utterance_get_language (utterance));
  g_assert_cmpfloat_with_epsilon (pitch, spiel_utterance_get_pitch (utterance),
                                  DBL_EPSILON);
  g_assert_cmpfloat_with_epsilon (rate, spiel_utterance_get_rate (utterance),
                                  DBL_EPSILON);
  g_assert_cmpstr (text, ==, spiel_utterance_get_text (utterance));
  g_assert_true (voice == spiel_utterance_get_voice (utterance));
  g_assert_cmpfloat_with_epsilon (
      volume, spiel_utterance_get_volume (utterance), DBL_EPSILON);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libspiel/utterance/new", test_utterance_new);
  g_test_add_func ("/libspiel/utterance/basic", test_utterance_properties);

  return g_test_run ();
}
