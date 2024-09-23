// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright © 2024 GNOME Foundation Inc.
// SPDX-FileContributor: Andy Holmes <andyholmes@gnome.org>

#include <gio/gio.h>
#include <spiel.h>

static void
test_provider_properties (void)
{
  g_autoptr (SpielSpeaker) speaker = NULL;
  g_autoptr (SpielProvider) provider = NULL;
  GError *error = NULL;
  g_autofree char *name_out = NULL;
  g_autoptr (GListModel) voices_out = NULL;
  g_autofree char *well_known_name_out = NULL;
  GListModel *providers = NULL;

  speaker = spiel_speaker_new_sync (NULL, &error);
  g_assert_no_error (error);
  g_assert_true (SPIEL_IS_SPEAKER (speaker));

  providers = spiel_speaker_get_providers (speaker);
  g_assert_true (G_IS_LIST_MODEL (providers));
  g_assert_cmpuint (g_list_model_get_n_items (providers), ==, 1);

  provider = g_list_model_get_item (providers, 0);
  g_assert_true (SPIEL_IS_PROVIDER (provider));

  g_object_get (provider, "name", &name_out, "well-known-name",
                &well_known_name_out, "voices", &voices_out, NULL);
  g_assert_cmpstr (name_out, ==, "Mock Provider");
  g_assert_cmpstr (well_known_name_out, ==, "org.mock.Speech.Provider");
  g_assert_true (G_IS_LIST_MODEL (voices_out));
  g_assert_cmpuint (g_list_model_get_n_items (voices_out), ==, 3);
  g_assert_cmpuint (g_list_model_get_item_type (voices_out), ==,
                    SPIEL_TYPE_VOICE);

  g_assert_cmpstr (spiel_provider_get_name (provider), ==, "Mock Provider");
  g_assert_cmpstr (spiel_provider_get_well_known_name (provider), ==,
                   "org.mock.Speech.Provider");
  g_assert_true (G_IS_LIST_MODEL (spiel_provider_get_voices (provider)));
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libspiel/provider/properties", test_provider_properties);

  return g_test_run ();
}
