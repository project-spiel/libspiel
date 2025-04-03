/* spiel.c
 *
 * Copyright (C) 2024 Eitan Isaacson <eitan@monotonous.org>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <spiel.h>

static gboolean list_voices = FALSE;
static gboolean list_providers = FALSE;
static const char *voice_id = NULL;
static const char *provider_id = NULL;
static const char *lang = NULL;
static double pitch = 1;
static double rate = 1;
static double volume = 1;
static gboolean is_ssml = FALSE;

static GOptionEntry entries[] = {
  { "list-voices", 'V', 0, G_OPTION_ARG_NONE, &list_voices,
    "List available voices", NULL },
  { "list-providers", 'P', 0, G_OPTION_ARG_NONE, &list_providers,
    "List available speech providers", NULL },
  { "voice", 'v', 0, G_OPTION_ARG_STRING, &voice_id,
    "Voice ID to use with utterance (should specify provider too)", NULL },
  { "provider", 'p', 0, G_OPTION_ARG_STRING, &provider_id,
    "Provider ID of voice to use with utterance", NULL },
  { "language", 'l', 0, G_OPTION_ARG_STRING, &lang,
    "Language to use with utterance (specifying a voice overrides this)",
    NULL },
  { "pitch", 0, 0, G_OPTION_ARG_DOUBLE, &pitch,
    "Pitch of utterance (default: 1.0 range: [0.0 - 2.0])", NULL },
  { "rate", 0, 0, G_OPTION_ARG_DOUBLE, &rate,
    "Rate of utterance (default: 1.0 range: [0.1 - 10.0])", NULL },
  { "volume", 0, 0, G_OPTION_ARG_DOUBLE, &volume,
    "Volume of utterance (default: 1.0 range: [0.1 - 1.0])", NULL },
  { "ssml", 0, 0, G_OPTION_ARG_NONE, &is_ssml, "Utterance is SSML markup",
    NULL },
  { NULL }
};

static void
do_list_voices (SpielSpeaker *speaker)
{
  GListModel *voices = spiel_speaker_get_voices (speaker);
  guint voices_count = g_list_model_get_n_items (voices);

  g_print ("%-25s %-10s %-10s %s\n", "NAME", "LANGUAGES", "IDENTIFIER",
           "PROVIDER");
  for (guint i = 0; i < voices_count; i++)
    {
      g_autoptr (SpielVoice) voice =
          SPIEL_VOICE (g_list_model_get_object (voices, i));
      g_autoptr (SpielProvider) provider = spiel_voice_get_provider (voice);
      g_autofree char *languages =
          g_strjoinv (",", (char **) spiel_voice_get_languages (voice));
      g_print ("%-25s %-10s %-10s %s\n", spiel_voice_get_name (voice),
               languages, spiel_voice_get_identifier (voice),
               spiel_provider_get_identifier (provider));
    }
}

static void
do_list_providers (SpielSpeaker *speaker)
{
  GListModel *providers = spiel_speaker_get_providers (speaker);
  guint providers_count = g_list_model_get_n_items (providers);

  g_print ("%-30s %s\n", "NAME", "IDENTIFIER");
  for (guint i = 0; i < providers_count; i++)
    {
      g_autoptr (SpielProvider) provider =
          SPIEL_PROVIDER (g_list_model_get_object (providers, i));
      g_print ("%-30s %s\n", spiel_provider_get_name (provider),
               spiel_provider_get_identifier (provider));
    }
}

static void
speaking_cb (SpielSpeaker *speaker, GParamSpec *pspec, gpointer user_data)
{
  GMainLoop *loop = user_data;
  gboolean speaking = FALSE;
  g_object_get (speaker, "speaking", &speaking, NULL);

  if (!speaking)
    {
      g_main_loop_quit (loop);
    }
}

static SpielVoice *
find_voice (SpielSpeaker *speaker)
{
  if (!voice_id && provider_id)
    {
      // Get first voice of provider
      GListModel *providers = spiel_speaker_get_providers (speaker);
      g_autoptr (SpielProvider) provider =
          SPIEL_PROVIDER (g_list_model_get_object (providers, 0));
      if (provider)
        {
          GListModel *voices = spiel_provider_get_voices (provider);
          return SPIEL_VOICE (g_list_model_get_object (voices, 0));
        }

      return NULL;
    }

  if (voice_id)
    {
      GListModel *voices = spiel_speaker_get_voices (speaker);
      guint voices_count = g_list_model_get_n_items (voices);

      for (guint i = 0; i < voices_count; i++)
        {
          g_autoptr (SpielVoice) voice =
              SPIEL_VOICE (g_list_model_get_object (voices, i));
          g_autoptr (SpielProvider) provider = spiel_voice_get_provider (voice);
          if (g_str_equal (voice_id, spiel_voice_get_identifier (voice)) &&
              (!provider_id ||
               g_str_equal (provider_id,
                            spiel_provider_get_identifier (provider))))
            {
              return g_steal_pointer (&voice);
            }
        }
    }

  return NULL;
}

static void
do_speak (SpielSpeaker *speaker, const char *utterance_text)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  g_autoptr (SpielUtterance) utterance = spiel_utterance_new (utterance_text);
  g_autoptr (SpielVoice) voice = find_voice (speaker);

  if (voice)
    {
      spiel_utterance_set_voice (utterance, voice);
    }

  if (lang)
    {
      spiel_utterance_set_language (utterance, lang);
    }

  spiel_utterance_set_pitch (utterance, pitch);
  spiel_utterance_set_rate (utterance, rate);
  spiel_utterance_set_volume (utterance, volume);
  spiel_utterance_set_is_ssml (utterance, is_ssml);

  g_signal_connect (speaker, "notify::speaking", G_CALLBACK (speaking_cb),
                    loop);

  spiel_speaker_speak (speaker, utterance);

  g_main_loop_run (loop);
}

int
main (int argc, char *argv[])
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GOptionContext) context;
  g_autoptr (SpielSpeaker) speaker;

  context = g_option_context_new ("- command line speech synthesis");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      return 1;
    }

  speaker = spiel_speaker_new_sync (NULL, &error);
  if (!speaker)
    {
      g_print ("failed in instantiate speaker: %s\n", error->message);
      return 1;
    }

  if (list_voices)
    {
      do_list_voices (speaker);
      return 0;
    }

  if (list_providers)
    {
      do_list_providers (speaker);
      return 0;
    }

  do_speak (speaker, argv[argc - 1]);
}
