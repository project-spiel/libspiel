
#include "libspiel.h"

#include "spiel-utterance.h"
#include "spiel-voice.h"

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
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/**
 * spiel_utterance_new: (constructor)
 * @text: (not nullable): The utterance text to be spoken.
 *
 * Creates a new #SpielUtterance.
 *
 * Returns: The new #NotifyNotification.
 */
SpielUtterance *
spiel_utterance_new (const char *text)
{
  return g_object_new (SPIEL_TYPE_UTTERANCE, "text", text, NULL);
}

static void
spiel_utterance_finalize (GObject *object)
{
  SpielUtterance *self = (SpielUtterance *) object;
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  g_free (priv->text);
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
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;
    case PROP_PITCH:
      g_value_set_double (value, priv->pitch);
      break;
    case PROP_RATE:
      g_value_set_double (value, priv->rate);
      break;
    case PROP_VOLUME:
      g_value_set_double (value, priv->volume);
      break;
    case PROP_VOICE:
      g_value_set_object (value, priv->voice);
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
  SpielUtterancePrivate *priv = spiel_utterance_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_free (priv->text);
      priv->text = g_strdup (g_value_get_string (value));
      g_object_notify (G_OBJECT (self), "text");
      break;
    case PROP_PITCH:
      priv->pitch = g_value_get_double (value);
      g_object_notify (G_OBJECT (self), "pitch");
      break;
    case PROP_RATE:
      priv->rate = g_value_get_double (value);
      g_object_notify (G_OBJECT (self), "rate");
      break;
    case PROP_VOLUME:
      priv->volume = g_value_get_double (value);
      g_object_notify (G_OBJECT (self), "volume");
      break;
    case PROP_VOICE:
      priv->voice = g_value_dup_object (value);
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

  properties[PROP_TEXT] = g_param_spec_string (
      "text", "speech text", "the utterance text that will be spoken",
      NULL /* default value */, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PITCH] =
      g_param_spec_double ("pitch", "speech pitch",
                           "the pitch at which the utterance will be spoken", 0,
                           2, 1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_RATE] = g_param_spec_double (
      "rate", "speech rate", "the speed at which the utterance will be spoken",
      0.1, 10, 1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_VOLUME] =
      g_param_spec_double ("volume", "speech volume",
                           "the volume at which the utterance will be spoken",
                           0, 1, 1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_VOICE] = g_param_spec_object (
      "voice", "speech voice",
      "the voice with which the utterance will be spoken", SPIEL_TYPE_VOICE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

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
}
