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

#include "libspiel.h"

#include "spiel-voice.h"
#include "spieldbusgenerated.h"

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
  char *provider_name;
} SpielVoicePrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (SpielVoice, spiel_voice, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_NAME,
  PROP_IDENTIFIER,
  PROP_LANGUAGES,
  PROP_PROVIDER_NAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/**
 * spiel_voice_get_name: (get-property name)
 * @self: a `SpielVoice`
 *
 * Fetches the name.
 *
 * Returns: the name text. This string is
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
 * @self: a `SpielVoice`
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
 * spiel_voice_get_provider_name: (get-property provider-name)
 * @self: a `SpielVoice`
 *
 * Fetches the provider name in the form of a unique DBus name.
 *
 * Returns: the provider name. This string is
 *   owned by the voice and must not be modified or freed.
 */
const char *
spiel_voice_get_provider_name (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  return priv->provider_name;
}

/**
 * spiel_voice_get_languages: (get-property languages)
 * @self: a `SpielVoice`
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

static void
spiel_voice_finalize (GObject *object)
{
  SpielVoice *self = (SpielVoice *) object;
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);

  g_free (priv->name);
  g_free (priv->identifier);
  g_strfreev (priv->languages);
  g_free (priv->provider_name);

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
    case PROP_PROVIDER_NAME:
      g_value_set_string (value, priv->provider_name);
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
      g_object_notify (G_OBJECT (self), "name");
      break;
    case PROP_IDENTIFIER:
      g_clear_pointer (&priv->identifier, g_free);
      priv->identifier = g_value_dup_string (value);
      g_object_notify (G_OBJECT (self), "identifier");
      break;
    case PROP_LANGUAGES:
      g_strfreev (priv->languages);
      priv->languages = g_value_dup_boxed (value);
      break;
    case PROP_PROVIDER_NAME:
      g_clear_pointer (&priv->provider_name, g_free);
      priv->provider_name = g_value_dup_string (value);
      g_object_notify (G_OBJECT (self), "provider-name");
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
   * SpielVoice:provider-name: (getter get_provider_name)
   *
   * The speech provider that implements this voice's DBus name.
   *
   */
  properties[PROP_PROVIDER_NAME] = g_param_spec_string (
      "provider-name", NULL, NULL, NULL /* default value */,
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
  priv->provider_name = NULL;
}
