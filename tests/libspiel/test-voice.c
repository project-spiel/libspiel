// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright © 2024 GNOME Foundation Inc.
// SPDX-FileContributor: Andy Holmes <andyholmes@gnome.org>

#include <gio/gio.h>
#include <spiel.h>

static void
test_voice_basic (void)
{
  g_autoptr (SpielVoice) voice = NULL;
  SpielVoiceFeature features = SPIEL_VOICE_FEATURE_EVENTS_SSML_MARK;
  const char *identifier = "gmw/en-US";
  const char *const languages[] = { "en-us", "en", NULL };
  const char *name = "English (America)";
  const char *output_format =
      "audio/x-spiel,format=S16LE,channels=1,rate=22050";
  g_autoptr (SpielProvider) provider = NULL;

  SpielVoiceFeature features_out = SPIEL_VOICE_FEATURE_NONE;
  g_autofree char *identifier_out = NULL;
  g_autofree char *name_out = NULL;
  g_auto (GStrv) languages_out = NULL;
  g_autoptr (SpielProvider) provider_out = NULL;

  voice = g_object_new (SPIEL_TYPE_VOICE, "features", features, "identifier",
                        identifier, "languages", languages, "name", name,
                        "provider", provider, NULL);
  spiel_voice_set_output_format (voice, output_format);

  g_object_get (voice, "features", &features_out, "identifier", &identifier_out,
                "languages", &languages_out, "name", &name_out, "provider",
                &provider_out, NULL);
  g_assert_cmpuint (features, ==, features_out);
  g_assert_cmpstr (identifier, ==, identifier_out);
  g_assert_true (g_strv_equal (languages, (const char *const *) languages_out));
  g_assert_cmpstr (name, ==, name_out);
  g_assert_true (provider == provider_out);

  g_clear_object (&provider);
  provider_out = spiel_voice_get_provider (voice);

  g_assert_cmpuint (features, ==, spiel_voice_get_features (voice));
  g_assert_cmpstr (identifier, ==, spiel_voice_get_identifier (voice));
  g_assert_true (g_strv_equal (languages, spiel_voice_get_languages (voice)));
  g_assert_cmpstr (name, ==, spiel_voice_get_name (voice));
  g_assert_cmpstr (output_format, ==, spiel_voice_get_output_format (voice));
  g_assert_true (provider == provider_out);
}

static void
test_voice_utils (void)
{
  g_autoptr (SpielProvider) provider1 = NULL;
  g_autoptr (SpielProvider) provider2 = NULL;
  g_autoptr (SpielVoice) voice1 = NULL;
  g_autoptr (SpielVoice) voice2 = NULL;
  g_autoptr (SpielVoice) voice3 = NULL;

  voice1 = g_object_new (SPIEL_TYPE_VOICE, "features", 242123, "identifier",
                         "gmw/en-US", "languages",
                         (const char *const[]){ "en-us", "en", NULL }, "name",
                         "English (America)", "provider", provider1, NULL);
  voice2 = g_object_new (SPIEL_TYPE_VOICE, "features", 242123, "identifier",
                         "gmw/en-US", "languages",
                         (const char *const[]){ "en-us", "en", NULL }, "name",
                         "English (America)", "provider", provider1, NULL);
  g_assert_true (spiel_voice_equal (voice1, voice2));

  voice3 = g_object_new (
      SPIEL_TYPE_VOICE, "features", 242123, "identifier", "gmw/en-GB",
      "languages", (const char *const[]){ "en-gb", "en", NULL }, "name",
      "English (Great Britain)", "provider", provider2, NULL);
  g_assert_false (spiel_voice_equal (voice2, voice3));

  g_assert_true (spiel_voice_hash (voice1) == spiel_voice_hash (voice2));
  g_assert_false (spiel_voice_hash (voice2) == spiel_voice_hash (voice3));

  // TODO: test prop ranges
  g_assert_cmpint (spiel_voice_compare (voice1, voice2, NULL), ==, 0);
  g_assert_cmpint (spiel_voice_compare (voice1, voice3, NULL), <, 0);
  g_assert_cmpint (spiel_voice_compare (voice3, voice2, NULL), >, 0);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libspiel/voice/basic", test_voice_basic);
  g_test_add_func ("/libspiel/voice/utils", test_voice_utils);

  return g_test_run ();
}
