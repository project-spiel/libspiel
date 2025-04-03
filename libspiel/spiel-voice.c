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

#include "spiel-provider.h"
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
  char *name;
  char *identifier;
  char **languages;
  char *output_format;
  SpielVoiceFeature features;
  GWeakRef provider;
};

G_DEFINE_FINAL_TYPE (SpielVoice, spiel_voice, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_NAME,
  PROP_IDENTIFIER,
  PROP_LANGUAGES,
  PROP_PROVIDER,
  PROP_FEATURES,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/**
 * spiel_voice_get_name: (get-property name)
 * @self: a `SpielVoice`
 *
 * Gets the name.
 *
 * Returns: (transfer none) (not nullable): the name
 *
 * Since: 1.0
 */
const char *
spiel_voice_get_name (SpielVoice *self)
{
  g_return_val_if_fail (SPIEL_IS_VOICE (self), NULL);

  return self->name;
}

/**
 * spiel_voice_get_identifier: (get-property identifier)
 * @self: a `SpielVoice`
 *
 * Gets the identifier, unique to [property@Spiel.Voice:provider].
 *
 * Returns: (transfer none) (not nullable): the identifier
 *
 * Since: 1.0
 */
const char *
spiel_voice_get_identifier (SpielVoice *self)
{
  g_return_val_if_fail (SPIEL_IS_VOICE (self), NULL);

  return self->identifier;
}

/**
 * spiel_voice_get_provider: (get-property provider)
 * @self: a `SpielVoice`
 *
 * Gets the provider associated with this voice
 *
 * Returns: (transfer full) (nullable): a `SpielProvider`
 *
 * Since: 1.0
 */
SpielProvider *
spiel_voice_get_provider (SpielVoice *self)
{
  g_return_val_if_fail (SPIEL_IS_VOICE (self), NULL);

  return g_weak_ref_get (&self->provider);
}

/**
 * spiel_voice_get_languages: (get-property languages)
 * @self: a `SpielVoice`
 *
 * Gets the list of supported languages.
 *
 * Returns: (transfer none) (not nullable): a list of BCP 47 tags
 *
 * Since: 1.0
 */
const char *const *
spiel_voice_get_languages (SpielVoice *self)
{
  g_return_val_if_fail (SPIEL_IS_VOICE (self), NULL);

  return (const char *const *) self->languages;
}

/**
 * spiel_voice_get_features: (get-property features)
 * @self: a `SpielVoice`
 *
 * Gets the list of supported features.
 *
 * Returns: a bit-field of `SpielVoiceFeature`
 *
 * Since: 1.0
 */
SpielVoiceFeature
spiel_voice_get_features (SpielVoice *self)
{
  g_return_val_if_fail (SPIEL_IS_VOICE (self), SPIEL_VOICE_FEATURE_NONE);

  return self->features;
}

/**
 * spiel_voice_get_output_format: (get-property output-format)
 * @self: a `SpielVoice`
 *
 * Gets the output format.
 *
 * Since: 1.0
 */
const char *
spiel_voice_get_output_format (SpielVoice *self)
{
  g_return_val_if_fail (SPIEL_IS_VOICE (self), NULL);

  return self->output_format;
}

/**
 * spiel_voice_set_output_format: (set-property output-format)
 * @self: a `SpielVoice`
 * @output_format: (not nullable): an output format string.
 *
 * Sets the audio output format.
 *
 * Since: 1.0
 */
void
spiel_voice_set_output_format (SpielVoice *self, const char *output_format)
{
  g_return_if_fail (SPIEL_IS_VOICE (self));
  g_return_if_fail (output_format != NULL && *output_format != '\0');

  g_clear_pointer (&self->output_format, g_free);
  self->output_format = g_strdup (output_format);
}

/**
 * spiel_voice_hash:
 * @self: (not nullable): a `SpielVoice`
 *
 * Converts a [class@Spiel.Voice] to a hash value.
 *
 * Returns: a hash value corresponding to @self
 *
 * Since: 1.0
 */
guint
spiel_voice_hash (SpielVoice *self)
{
  g_autoptr (SpielProvider) provider = NULL;
  guint hash = 0;

  g_return_val_if_fail (SPIEL_IS_VOICE (self), 0);

  provider = spiel_voice_get_provider (self);
  hash = g_str_hash (self->name);
  hash = (hash << 5) - hash + g_str_hash (self->identifier);
  if (provider)
    {
      hash = (hash << 5) - hash +
             g_str_hash (spiel_provider_get_identifier (provider));
    }

  for (char **language = self->languages; *language; language++)
    {
      hash = (hash << 5) - hash + g_str_hash (*language);
    }

  return hash;
}

/**
 * spiel_voice_equal:
 * @self: (not nullable): a `SpielVoice`
 * @other: (not nullable): a `SpielVoice` to compare with @self
 *
 * Compares the two [class@Spiel.Voice] values and returns %TRUE if equal.
 *
 * Returns: %TRUE if the two voices match.
 *
 * Since: 1.0
 */
gboolean
spiel_voice_equal (SpielVoice *self, SpielVoice *other)
{
  g_autoptr (SpielProvider) self_provider = NULL;
  g_autoptr (SpielProvider) other_provider = NULL;

  g_return_val_if_fail (SPIEL_IS_VOICE (self), FALSE);
  g_return_val_if_fail (SPIEL_IS_VOICE (other), FALSE);

  self_provider = g_weak_ref_get (&self->provider);
  other_provider = g_weak_ref_get (&other->provider);

  if (self_provider != other_provider)
    {
      return FALSE;
    }

  if (!g_str_equal (self->name, other->name))
    {
      return FALSE;
    }

  if (!g_str_equal (self->identifier, other->identifier))
    {
      return FALSE;
    }

  if (!g_strv_equal ((const gchar *const *) self->languages,
                     (const gchar *const *) other->languages))
    {
      return FALSE;
    }

  return TRUE;
}

/**
 * spiel_voice_compare:
 * @self: (not nullable): a `SpielVoice`
 * @other: (not nullable): a `SpielVoice` to compare with @self
 * @user_data: user-defined callback data
 *
 * Compares the two [class@Spiel.Voice] values and returns a negative integer
 * if the first value comes before the second, 0 if they are equal, or a
 * positive integer if the first value comes after the second.
 *
 * Returns: an integer indicating order
 *
 * Since: 1.0
 */
gint
spiel_voice_compare (SpielVoice *self, SpielVoice *other, gpointer user_data)
{
  g_autoptr (SpielProvider) self_provider = NULL;
  g_autoptr (SpielProvider) other_provider = NULL;
  int cmp = 0;

  g_return_val_if_fail (SPIEL_IS_VOICE (self), 0);
  g_return_val_if_fail (SPIEL_IS_VOICE (other), 0);

  self_provider = g_weak_ref_get (&self->provider);
  other_provider = g_weak_ref_get (&other->provider);

  if ((cmp = g_strcmp0 (
           self_provider ? spiel_provider_get_identifier (self_provider) : "",
           other_provider ? spiel_provider_get_identifier (other_provider)
                          : "")))
    {
      return cmp;
    }

  if ((cmp = g_strcmp0 (self->name, other->name)))
    {
      return cmp;
    }

  if ((cmp = g_strcmp0 (self->identifier, other->identifier)))
    {
      return cmp;
    }

  return cmp;
}

static void
spiel_voice_finalize (GObject *object)
{
  SpielVoice *self = (SpielVoice *) object;

  g_free (self->name);
  g_free (self->identifier);
  g_strfreev (self->languages);
  g_free (self->output_format);
  g_weak_ref_clear (&self->provider);

  G_OBJECT_CLASS (spiel_voice_parent_class)->finalize (object);
}

static void
spiel_voice_get_property (GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
  SpielVoice *self = SPIEL_VOICE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_IDENTIFIER:
      g_value_set_string (value, self->identifier);
      break;
    case PROP_LANGUAGES:
      g_value_set_boxed (value, self->languages);
      break;
    case PROP_PROVIDER:
      g_value_take_object (value, spiel_voice_get_provider (self));
      break;
    case PROP_FEATURES:
      g_value_set_flags (value, self->features);
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

  switch (prop_id)
    {
    case PROP_NAME:
      g_clear_pointer (&self->name, g_free);
      self->name = g_value_dup_string (value);
      break;
    case PROP_IDENTIFIER:
      g_clear_pointer (&self->identifier, g_free);
      self->identifier = g_value_dup_string (value);
      break;
    case PROP_LANGUAGES:
      g_strfreev (self->languages);
      self->languages = g_value_dup_boxed (value);
      break;
    case PROP_PROVIDER:
      g_weak_ref_set (&self->provider, g_value_get_object (value));
      break;
    case PROP_FEATURES:
      self->features = g_value_get_flags (value);
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
   * Since: 1.0
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
   * Since: 1.0
   */
  properties[PROP_IDENTIFIER] = g_param_spec_string (
      "identifier", NULL, NULL, NULL /* default value */,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielVoice:languages: (getter get_languages)
   *
   * A list of supported languages encoded as BCP 47 tags.
   *
   * Since: 1.0
   */
  properties[PROP_LANGUAGES] = g_param_spec_boxed (
      "languages", NULL, NULL, G_TYPE_STRV,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielVoice:provider: (getter get_provider)
   *
   * The speech provider that implements this voice.
   *
   * Since: 1.0
   */
  properties[PROP_PROVIDER] = g_param_spec_object (
      "provider", NULL, NULL, SPIEL_TYPE_PROVIDER,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielVoice:features: (getter get_features)
   *
   * A bitfield of supported features.
   *
   * Since: 1.0
   */
  properties[PROP_FEATURES] = g_param_spec_flags (
      "features", NULL, NULL, SPIEL_TYPE_VOICE_FEATURE,
      SPIEL_VOICE_FEATURE_NONE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
spiel_voice_init (SpielVoice *self)
{
  self->name = NULL;
  self->identifier = NULL;
  self->languages = NULL;
  self->output_format = NULL;
  self->features = 0;
  g_weak_ref_init (&self->provider, NULL);
}
