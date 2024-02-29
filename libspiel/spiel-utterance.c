/* spiel-utterance.c
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

#include "spiel-utterance.h"

/**
 * SpielUtterance:
 *
 * Represents an utterance to be spoken by a `SpielSpeaker`.
 *
 * An utterance consists of the text to be spoken and other properties that
 * affect the speech, like rate, pitch or voice used.
 *
 * Since: 1.0
 */

struct _SpielUtterance
{
  GObject parent_instance;
};

typedef struct
{
  char *text;
  double pitch;
  double rate;
  double volume;
  SpielVoice *voice;
  char *language;
  gboolean is_ssml;
} SpielUtterancePrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (SpielUtterance,
                                  spiel_utterance,
                                  G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_TEXT,
  PROP_PITCH,
  PROP_RATE,
  PROP_VOLUME,
  PROP_VOICE,
  PROP_LANGUAGE,
  PROP_IS_SSML,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/**
 * spiel_utterance_new: (constructor)
 * @text: (nullable): The utterance text to be spoken.
 *
 * Creates a new [class@Spiel.Utterance].
 *
 * Returns: The new `SpielUtterance`.
 *
 * Since: 1.0
 */
SpielUtterance *
spiel_utterance_new (const char *text)
{
  return g_object_new (SPIEL_TYPE_UTTERANCE, "text", text, NULL);
}

/**
 * spiel_utterance_get_text: (get-property text)
 * @self: a `SpielUtterance`
 *
 * Gets the text spoken in this utterance.
 *
 * Returns: (transfer none): the text
 *
 * Since: 1.0
 */
const char *
spiel_utterance_get_text (SpielUtterance *self)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_val_if_fail (SPIEL_IS_UTTERANCE (self), NULL);

  return priv->text;
}

/**
 * spiel_utterance_set_text: (set-property text)
 * @self: a `SpielUtterance`
 * @text: the text to assign to this utterance
 *
 * Sets the text to be spoken by this utterance.
 *
 * Since: 1.0
 */
void
spiel_utterance_set_text (SpielUtterance *self, const char *text)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_if_fail (SPIEL_IS_UTTERANCE (self));

  g_free (priv->text);
  priv->text = g_strdup (text);
  g_object_notify (G_OBJECT (self), "text");
}

/**
 * spiel_utterance_get_pitch: (get-property pitch)
 * @self: a `SpielUtterance`
 *
 * Gets the pitch used in this utterance.
 *
 * Returns: the pitch value
 *
 * Since: 1.0
 */
double
spiel_utterance_get_pitch (SpielUtterance *self)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_val_if_fail (SPIEL_IS_UTTERANCE (self), 1.0);

  return priv->pitch;
}

/**
 * spiel_utterance_set_pitch: (set-property pitch)
 * @self: a `SpielUtterance`
 * @pitch: a pitch to assign to this utterance
 *
 * Sets a pitch on this utterance.
 *
 * Since: 1.0
 */
void
spiel_utterance_set_pitch (SpielUtterance *self, double pitch)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_if_fail (SPIEL_IS_UTTERANCE (self));

  priv->pitch = pitch;
  g_object_notify (G_OBJECT (self), "pitch");
}

/**
 * spiel_utterance_get_rate: (get-property rate)
 * @self: a `SpielUtterance`
 *
 * Gets the rate used in this utterance.
 *
 * Returns: the rate value
 *
 * Since: 1.0
 */
double
spiel_utterance_get_rate (SpielUtterance *self)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_val_if_fail (SPIEL_IS_UTTERANCE (self), 1.0);

  return priv->rate;
}

/**
 * spiel_utterance_set_rate: (set-property rate)
 * @self: a `SpielUtterance`
 * @rate: a rate to assign to this utterance
 *
 * Sets a rate on this utterance.
 *
 * Since: 1.0
 */
void
spiel_utterance_set_rate (SpielUtterance *self, double rate)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_if_fail (SPIEL_IS_UTTERANCE (self));

  priv->rate = rate;
  g_object_notify (G_OBJECT (self), "rate");
}

/**
 * spiel_utterance_get_volume: (get-property volume)
 * @self: a `SpielUtterance`
 *
 * Gets the volume used in this utterance.
 *
 * Returns: the volume value
 *
 * Since: 1.0
 */
double
spiel_utterance_get_volume (SpielUtterance *self)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_val_if_fail (SPIEL_IS_UTTERANCE (self), 1.0);

  return priv->volume;
}

/**
 * spiel_utterance_set_volume: (set-property volume)
 * @self: a `SpielUtterance`
 * @volume: a volume to assign to this utterance
 *
 * Sets a volume on this utterance.
 *
 * Since: 1.0
 */
void
spiel_utterance_set_volume (SpielUtterance *self, double volume)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_if_fail (SPIEL_IS_UTTERANCE (self));

  priv->volume = volume;
  g_object_notify (G_OBJECT (self), "volume");
}

/**
 * spiel_utterance_get_voice: (get-property voice)
 * @self: a `SpielUtterance`
 *
 * Gets the voice used in this utterance
 *
 * Returns: (transfer none) (nullable): a `SpielVoice`
 *
 * Since: 1.0
 */
SpielVoice *
spiel_utterance_get_voice (SpielUtterance *self)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_val_if_fail (SPIEL_IS_UTTERANCE (self), NULL);

  return priv->voice;
}

/**
 * spiel_utterance_set_voice: (set-property voice)
 * @self: a `SpielUtterance`
 * @voice: a `SpielVoice` to assign to this utterance
 *
 * Sets a voice on this utterance.
 *
 * Since: 1.0
 */
void
spiel_utterance_set_voice (SpielUtterance *self, SpielVoice *voice)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_if_fail (SPIEL_IS_UTTERANCE (self));
  g_return_if_fail (voice == NULL || SPIEL_IS_VOICE (voice));

  g_clear_object (&(priv->voice));

  priv->voice = voice ? g_object_ref (voice) : NULL;
  g_object_notify (G_OBJECT (self), "voice");
}

/**
 * spiel_utterance_get_language: (get-property language)
 * @self: a `SpielUtterance`
 *
 * Gets the language used in this utterance.
 *
 * Returns: (transfer none): the language
 *
 * Since: 1.0
 */
const char *
spiel_utterance_get_language (SpielUtterance *self)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_val_if_fail (SPIEL_IS_UTTERANCE (self), NULL);

  return priv->language;
}

/**
 * spiel_utterance_set_language: (set-property language)
 * @self: a `SpielUtterance`
 * @language: the language to assign to this utterance
 *
 * Sets the language of this utterance
 *
 * Since: 1.0
 */
void
spiel_utterance_set_language (SpielUtterance *self, const char *language)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_if_fail (SPIEL_IS_UTTERANCE (self));

  g_free (priv->language);
  priv->language = g_strdup (language);
  g_object_notify (G_OBJECT (self), "language");
}

/**
 * spiel_utterance_get_is_ssml: (get-property is-ssml)
 * @self: a `SpielUtterance`
 *
 * Gets whether the current utterance an SSML snippet.
 *
 * Returns: %TRUE if the utterance text is SSML
 *
 * Since: 1.0
 */
gboolean
spiel_utterance_get_is_ssml (SpielUtterance *self)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_val_if_fail (SPIEL_IS_UTTERANCE (self), FALSE);

  return priv->is_ssml;
}

/**
 * spiel_utterance_set_is_ssml: (set-property is-ssml)
 * @self: a `SpielUtterance`
 * @is_ssml: whether the utterance text is an SSML snippet
 *
 * Indicates whether this utterance should be interpreted as SSML.
 *
 * Since: 1.0
 */
void
spiel_utterance_set_is_ssml (SpielUtterance *self, gboolean is_ssml)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_return_if_fail (SPIEL_IS_UTTERANCE (self));

  priv->is_ssml = is_ssml;
}

static void
spiel_utterance_finalize (GObject *object)
{
  SpielUtterance *self = (SpielUtterance *) object;
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_free (priv->text);
  g_free (priv->language);
  g_clear_object (&(priv->voice));

  G_OBJECT_CLASS (spiel_utterance_parent_class)->finalize (object);
}

static void
spiel_utterance_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  SpielUtterance *self = SPIEL_UTTERANCE (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, spiel_utterance_get_text (self));
      break;
    case PROP_PITCH:
      g_value_set_double (value, spiel_utterance_get_pitch (self));
      break;
    case PROP_RATE:
      g_value_set_double (value, spiel_utterance_get_rate (self));
      break;
    case PROP_VOLUME:
      g_value_set_double (value, spiel_utterance_get_volume (self));
      break;
    case PROP_VOICE:
      g_value_set_object (value, spiel_utterance_get_voice (self));
      break;
    case PROP_LANGUAGE:
      g_value_set_string (value, spiel_utterance_get_language (self));
      break;
    case PROP_IS_SSML:
      g_value_set_boolean (value, spiel_utterance_get_is_ssml (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spiel_utterance_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  SpielUtterance *self = SPIEL_UTTERANCE (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      spiel_utterance_set_text (self, g_value_get_string (value));
      break;
    case PROP_PITCH:
      spiel_utterance_set_pitch (self, g_value_get_double (value));
      break;
    case PROP_RATE:
      spiel_utterance_set_rate (self, g_value_get_double (value));
      break;
    case PROP_VOLUME:
      spiel_utterance_set_volume (self, g_value_get_double (value));
      break;
    case PROP_VOICE:
      spiel_utterance_set_voice (self, g_value_dup_object (value));
      break;
    case PROP_LANGUAGE:
      spiel_utterance_set_language (self, g_value_get_string (value));
      break;
    case PROP_IS_SSML:
      spiel_utterance_set_is_ssml (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spiel_utterance_class_init (SpielUtteranceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_utterance_finalize;
  object_class->get_property = spiel_utterance_get_property;
  object_class->set_property = spiel_utterance_set_property;

  /**
   * SpielUtterance:text: (getter get_text) (setter set_text)
   *
   * The utterance text that will be spoken.
   *
   * Since: 1.0
   */
  properties[PROP_TEXT] =
      g_param_spec_string ("text", NULL, NULL, NULL /* default value */,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielUtterance:pitch: (getter get_pitch) (setter set_pitch)
   *
   * The pitch at which the utterance will be spoken.
   *
   * Since: 1.0
   */
  properties[PROP_PITCH] = g_param_spec_double (
      "pitch", NULL, NULL, 0, 2, 1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielUtterance:rate: (getter get_rate) (setter set_rate)
   *
   * The speed at which the utterance will be spoken.
   *
   * Since: 1.0
   */
  properties[PROP_RATE] =
      g_param_spec_double ("rate", NULL, NULL, 0.1, 10, 1,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielUtterance:volume: (getter get_volume) (setter set_volume)
   *
   * The volume at which the utterance will be spoken.
   *
   * Since: 1.0
   */
  properties[PROP_VOLUME] =
      g_param_spec_double ("volume", NULL, NULL, 0, 1, 1,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielUtterance:voice: (getter get_voice) (setter set_voice)
   *
   * The voice with which the utterance will be spoken.
   *
   * Since: 1.0
   */
  properties[PROP_VOICE] =
      g_param_spec_object ("voice", NULL, NULL, SPIEL_TYPE_VOICE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielUtterance:language: (getter get_language) (setter set_language)
   *
   * The utterance language. If no voice is set this language will be used to
   * select the best matching voice.
   *
   * Since: 1.0
   */
  properties[PROP_LANGUAGE] =
      g_param_spec_string ("language", NULL, NULL, NULL /* default value */,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielUtterance:is-ssml: (getter get_is_ssml) (setter set_is_ssml)
   *
   * Whether the utterance's text should be interpreted as an SSML snippet.
   *
   * Since: 1.0
   */
  properties[PROP_IS_SSML] = g_param_spec_boolean (
      "is-ssml", NULL, NULL, FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
spiel_utterance_init (SpielUtterance *self)
{
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);
  priv->text = NULL;
  priv->rate = 1;
  priv->volume = 1;
  priv->pitch = 1;
  priv->voice = NULL;
  priv->language = NULL;
  priv->is_ssml = FALSE;
}
