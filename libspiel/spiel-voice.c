/* spiel-voice.c
 *
 * Copyright (C) 2023 Eitan Isaacson <eitan@monotonous.org>
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

#include "spiel.h"

#include "spiel-provider-proxy.h"
#include "spiel-voice.h"

/**
 * SpielVoice:
 *
 * Represents a voice implemented by a speech provider.
 *
 * A DBus speech provider advertises a list of voices that it implements.
 * Each voice will have human-readable name, a unique identifier, and a list
 * of languages supported by the voice.
 *
 */

struct _SpielVoice
{
  GObject parent_instance;
};

typedef struct
{
  char *name;
  char *identifier;
  char **languages;
  char *provider_well_known_name;
  char *output_format;
  SpielVoiceFeature features;
} SpielVoicePrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (SpielVoice, spiel_voice, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_NAME,
  PROP_IDENTIFIER,
  PROP_LANGUAGES,
  PROP_PROVIDER_WELL_KNOWN_NAME,
  PROP_FEATURES,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/**
 * spiel_voice_get_name: (get-property name)
 * @self: a #SpielVoice
 *
 * Fetches the name.
 *
 * Returns: (transfer none): the name text. This string is
 *   owned by the voice and must not be modified or freed.
 */
const char *
spiel_voice_get_name (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  return priv->name;
}

/**
 * spiel_voice_get_identifier: (get-property identifier)
 * @self: a #SpielVoice
 *
 * Fetches the identifier.
 *
 * Returns: the identifier text. This string is
 *   owned by the voice and must not be modified or freed.
 */
const char *
spiel_voice_get_identifier (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  return priv->identifier;
}

/**
 * spiel_voice_get_provider_well_known_name: (get-property
 * provider-well-known-name)
 * @self: a #SpielVoice
 *
 * Fetches the provider well known name in the form of a unique DBus name.
 *
 * Returns: the provider well known name. This string is
 *   owned by the voice and must not be modified or freed.
 */
const char *
spiel_voice_get_provider_well_known_name (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  return priv->provider_well_known_name;
}

/**
 * spiel_voice_get_languages: (get-property languages)
 * @self: a #SpielVoice
 *
 * Fetches the list of supported languages
 *
 * Returns: the list of supported languages. This list is
 *   owned by the voice and must not be modified or freed.
 */
const char *const *
spiel_voice_get_languages (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  return (const char *const *) priv->languages;
}

SpielVoiceFeature
spiel_voice_get_features (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  return priv->features;
}

const char *
spiel_voice_get_output_format (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  return priv->output_format;
}

void
spiel_voice_set_output_format (SpielVoice *self, const char *output_format)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  g_clear_pointer (&priv->output_format, g_free);
  priv->output_format = g_strdup (output_format);
}

guint
spiel_voice_hash (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  guint hash = 0;

  hash = g_str_hash (priv->name);
  hash = (hash << 5) - hash + g_str_hash (priv->identifier);
  hash = (hash << 5) - hash + g_str_hash (priv->provider_well_known_name);

  for (char **language = priv->languages; *language; language++)
    {
      hash = (hash << 5) - hash + g_str_hash (*language);
    }

  return hash;
}

gboolean
spiel_voice_equal (SpielVoice *self, SpielVoice *other)
{
  SpielVoicePrivate *self_priv = spiel_voice_get_instance_private (self);
  SpielVoicePrivate *other_priv = spiel_voice_get_instance_private (other);

  if (!g_str_equal (self_priv->provider_well_known_name,
                    other_priv->provider_well_known_name))
    {
      return FALSE;
    }

  if (!g_str_equal (self_priv->name, other_priv->name))
    {
      return FALSE;
    }

  if (!g_str_equal (self_priv->identifier, other_priv->identifier))
    {
      return FALSE;
    }

  if (!g_strv_equal ((const gchar *const *) self_priv->languages,
                     (const gchar *const *) other_priv->languages))
    {
      return FALSE;
    }

  return TRUE;
}

gint
spiel_voice_compare (SpielVoice *self, SpielVoice *other, gpointer user_data)
{
  SpielVoicePrivate *self_priv = spiel_voice_get_instance_private (self);
  SpielVoicePrivate *other_priv = spiel_voice_get_instance_private (other);
  gint cmp = 0;

  if ((cmp = g_strcmp0 (self_priv->provider_well_known_name,
                        other_priv->provider_well_known_name)))
    {
      return cmp;
    }

  if ((cmp = g_strcmp0 (self_priv->name, other_priv->name)))
    {
      return cmp;
    }

  if ((cmp = g_strcmp0 (self_priv->identifier, other_priv->identifier)))
    {
      return cmp;
    }

  return cmp;
}

static void
spiel_voice_finalize (GObject *object)
{
  SpielVoice *self = (SpielVoice *) object;
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);

  g_free (priv->name);
  g_free (priv->identifier);
  g_strfreev (priv->languages);
  g_free (priv->provider_well_known_name);

  G_OBJECT_CLASS (spiel_voice_parent_class)->finalize (object);
}

static void
spiel_voice_get_property (GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
  SpielVoice *self = SPIEL_VOICE (object);
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_IDENTIFIER:
      g_value_set_string (value, priv->identifier);
      break;
    case PROP_LANGUAGES:
      g_value_set_boxed (value, priv->languages);
      break;
    case PROP_PROVIDER_WELL_KNOWN_NAME:
      g_value_set_string (value, priv->provider_well_known_name);
      break;
    case PROP_FEATURES:
      g_value_set_flags (value, priv->features);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spiel_voice_set_property (GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
  SpielVoice *self = SPIEL_VOICE (object);
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_NAME:
      g_clear_pointer (&priv->name, g_free);
      priv->name = g_value_dup_string (value);
      break;
    case PROP_IDENTIFIER:
      g_clear_pointer (&priv->identifier, g_free);
      priv->identifier = g_value_dup_string (value);
      break;
    case PROP_LANGUAGES:
      g_strfreev (priv->languages);
      priv->languages = g_value_dup_boxed (value);
      break;
    case PROP_PROVIDER_WELL_KNOWN_NAME:
      g_clear_pointer (&priv->provider_well_known_name, g_free);
      priv->provider_well_known_name = g_value_dup_string (value);
      break;
    case PROP_FEATURES:
      priv->features = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spiel_voice_class_init (SpielVoiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_voice_finalize;
  object_class->get_property = spiel_voice_get_property;
  object_class->set_property = spiel_voice_set_property;

  /**
   * SpielVoice:name: (getter get_name)
   *
   * A human readable name for the voice. Not guaranteed to be unique.
   * May, or may not, be localized by the speech provider.
   *
   */
  properties[PROP_NAME] = g_param_spec_string (
      "name", NULL, NULL, NULL /* default value */,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielVoice:identifier: (getter get_identifier)
   *
   * A unique identifier of the voice. The uniqueness should be considered
   * in the scope of the provider (ie. two providers can use the same
   * identifier).
   *
   */
  properties[PROP_IDENTIFIER] = g_param_spec_string (
      "identifier", NULL, NULL, NULL /* default value */,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielVoice:languages: (getter get_languages)
   *
   * A list of supported languages encoded as BCP 47 tags.
   *
   */
  properties[PROP_LANGUAGES] = g_param_spec_boxed (
      "languages", NULL, NULL, G_TYPE_STRV,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielVoice:provider-well-known-name: (getter get_provider_well_known_name)
   *
   * The speech provider that implements this voice's DBus name.
   *
   */
  properties[PROP_PROVIDER_WELL_KNOWN_NAME] = g_param_spec_string (
      "provider-well-known-name", NULL, NULL, NULL /* default value */,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_FEATURES] = g_param_spec_flags (
      "features", NULL, NULL, SPIEL_TYPE_VOICE_FEATURE,
      SPIEL_VOICE_FEATURE_NONE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
spiel_voice_init (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);

  priv->name = NULL;
  priv->identifier = NULL;
  priv->languages = NULL;
  priv->provider_well_known_name = NULL;
  priv->output_format = NULL;
  priv->features = 0;
}
