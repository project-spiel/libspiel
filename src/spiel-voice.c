
#include "libspiel.h"

#include "spiel-voice.h"
#include "spieldbusgenerated.h"

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

SpielVoice *
spiel_voice_new (const char *name,
                 const char *identifier,
                 const char **languages,
                 const char *provider_name)
{
  return g_object_new (SPIEL_TYPE_VOICE, "name", name, "identifier", identifier,
                       "languages", languages, "provider-name", provider_name,
                       NULL);
}

const char *
spiel_voice_get_identifier (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  return priv->identifier;
}

const char *
spiel_voice_get_provider_name (SpielVoice *self)
{
  SpielVoicePrivate *priv = spiel_voice_get_instance_private (self);
  return priv->provider_name;
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

  properties[PROP_NAME] = g_param_spec_string (
      "name", NULL, NULL, NULL /* default value */,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_IDENTIFIER] = g_param_spec_string (
      "identifier", NULL, NULL, NULL /* default value */,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_LANGUAGES] = g_param_spec_boxed (
      "languages", NULL, NULL, G_TYPE_STRV,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

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
