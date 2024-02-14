/* spiel-provider.c
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

#include "spiel-collect-providers.h"
#include "spiel-provider-private.h"
#include "spiel-provider-proxy.h"
#include "spiel-provider.h"

/**
 * SpielProvider:
 *
 * Represents a provider speech backend.
 *
 */

struct _SpielProvider
{
  GObject parent_instance;
};

typedef struct
{
  SpielProviderProxy *provider_proxy;
  gboolean is_activatable;
  GListStore *voices;
  GHashTable *voices_hashset;
  gulong voices_changed_handler_id;
} SpielProviderPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (SpielProvider, spiel_provider, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_WELL_KNOWN_NAME,
  PROP_NAME,
  PROP_VOICES,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean handle_voices_changed (SpielProviderProxy *provider_proxy,
                                       GParamSpec *spec,
                                       gpointer user_data);

static void _spiel_provider_update_voices (SpielProvider *self);

SpielProvider *
spiel_provider_new (void)
{
  return g_object_new (SPIEL_TYPE_PROVIDER, NULL);
}

void
spiel_provider_set_proxy (SpielProvider *self,
                          SpielProviderProxy *provider_proxy)
{
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);
  g_assert (!priv->provider_proxy);

  priv->provider_proxy = g_object_ref (provider_proxy);

  _spiel_provider_update_voices (self);

  priv->voices_changed_handler_id =
      g_signal_connect (priv->provider_proxy, "notify::voices",
                        G_CALLBACK (handle_voices_changed), self);
}

SpielProviderProxy *
spiel_provider_get_proxy (SpielProvider *self)
{
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);
  return priv->provider_proxy;
}

SpielVoice *
spiel_provider_get_voice_by_id (SpielProvider *self, const char *voice_id)
{
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);
  guint voices_count = g_list_model_get_n_items (G_LIST_MODEL (priv->voices));

  for (guint i = 0; i < voices_count; i++)
    {
      SpielVoice *voice = SPIEL_VOICE (
          g_list_model_get_object (G_LIST_MODEL (priv->voices), i));
      if (g_str_equal (spiel_voice_get_identifier (voice), voice_id))
        {
          return voice;
        }
      g_object_unref (voice); // Just want to borrow a ref.
    }
  return NULL;
}

/**
 * spiel_provider_proxy_get_name: (get-property name)
 * @self: a #SpielProvider
 *
 * Fetches a human readable name of this provider
 *
 * Returns: (transfer none): the human readable name.
 */
const char *
spiel_provider_get_name (SpielProvider *self)
{
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);
  g_return_val_if_fail (priv->provider_proxy, NULL);

  return spiel_provider_proxy_get_name (priv->provider_proxy);
}

/**
 * spiel_provider_get_well_known_name: (get-property well-known-name)
 * @self: a #SpielProvider
 *
 * Fetches the provider's D-Bus well known name.
 *
 * Returns: (transfer none): the well known name.
 */
const char *
spiel_provider_get_well_known_name (SpielProvider *self)
{
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);
  g_return_val_if_fail (priv->provider_proxy, NULL);

  return g_dbus_proxy_get_name (G_DBUS_PROXY (priv->provider_proxy));
}

/**
 * spiel_provider_get_voices: (get-property voices)
 * @self: a #SpielProvider
 * *
 * Fetches the provider's voices.
 *
 * Returns: (transfer none): A list of #SpielVoice voices provided
 */
GListModel *
spiel_provider_get_voices (SpielProvider *self)
{
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);

  return G_LIST_MODEL (priv->voices);
}

void
spiel_provider_set_is_activatable (SpielProvider *self, gboolean is_activatable)
{
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);
  priv->is_activatable = is_activatable;
}

gboolean
spiel_provider_get_is_activatable (SpielProvider *self)
{
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);
  return priv->is_activatable;
}

static GSList *
_create_provider_voices (SpielProviderProxy *provider_proxy)
{
  const char *provider_name =
      g_dbus_proxy_get_name (G_DBUS_PROXY (provider_proxy));
  GSList *voices_slist = NULL;
  GVariant *voices = spiel_provider_proxy_get_voices (provider_proxy);
  gsize voices_count = voices ? g_variant_n_children (voices) : 0;

  for (gsize i = 0; i < voices_count; i++)
    {
      const char *name = NULL;
      const char *identifier = NULL;
      const char *output_format = NULL;
      const char **languages = NULL;
      guint64 features;
      SpielVoice *voice;

      g_variant_get_child (voices, i, "(&s&s&st^a&s)", &name, &identifier,
                           &output_format, &features, &languages);
      if (features >> 32)
        {
          g_warning ("Voice features past 32 bits are ignored in %s (%s)",
                     identifier, provider_name);
        }
      voice = g_object_new (SPIEL_TYPE_VOICE, "name", name, "identifier",
                            identifier, "languages", languages,
                            "provider-well-known-name", provider_name,
                            "features", features, NULL);
      spiel_voice_set_output_format (voice, output_format);

      voices_slist = g_slist_prepend (voices_slist, voice);
      g_free (languages);
    }

  return voices_slist;
}

static void
_spiel_provider_update_voices (SpielProvider *self)
{
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);
  GSList *new_voices = NULL;
  GHashTable *new_voices_hashset = NULL;

  g_return_if_fail (priv->provider_proxy);

  new_voices = _create_provider_voices (priv->provider_proxy);
  if (g_hash_table_size (priv->voices_hashset) > 0)
    {
      // We are adding voices to an already populated provider_proxy, store
      // new voices in a hashset for easy purge of ones that were removed.
      new_voices_hashset = g_hash_table_new ((GHashFunc) spiel_voice_hash,
                                             (GCompareFunc) spiel_voice_equal);
    }

  if (new_voices)
    {
      for (GSList *item = new_voices; item; item = item->next)
        {
          SpielVoice *voice = item->data;
          if (!g_hash_table_contains (priv->voices_hashset, voice))
            {
              g_hash_table_insert (priv->voices_hashset, g_object_ref (voice),
                                   NULL);
              g_list_store_insert_sorted (
                  priv->voices, g_object_ref (voice),
                  (GCompareDataFunc) spiel_voice_compare, NULL);
            }
          if (new_voices_hashset)
            {
              g_hash_table_insert (new_voices_hashset, voice, NULL);
            }
        }
    }

  if (new_voices_hashset)
    {
      GHashTableIter voices_iter;
      SpielVoice *old_voice;
      g_hash_table_iter_init (&voices_iter, priv->voices_hashset);
      while (
          g_hash_table_iter_next (&voices_iter, (gpointer *) &old_voice, NULL))
        {
          if (!g_hash_table_contains (new_voices_hashset, old_voice))
            {
              guint position = 0;
              if (g_list_store_find (priv->voices, old_voice, &position))
                {
                  g_list_store_remove (priv->voices, position);
                }
              g_hash_table_iter_remove (&voices_iter);
            }
        }
      g_hash_table_unref (new_voices_hashset);
    }

  g_slist_free_full (new_voices, (GDestroyNotify) g_object_unref);
}

static gboolean
handle_voices_changed (SpielProviderProxy *provider_proxy,
                       GParamSpec *spec,
                       gpointer user_data)
{
  SpielProvider *self = user_data;
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);
  char *name_owner =
      g_dbus_proxy_get_name_owner (G_DBUS_PROXY (priv->provider_proxy));

  if (name_owner == NULL && priv->is_activatable)
    {
      // Got a change notification because an activatable service left the bus.
      // Its voices are still valid, though.
      return TRUE;
    }

  g_free (name_owner);

  _spiel_provider_update_voices (self);

  return TRUE;
}

gint
spiel_provider_compare (SpielProvider *self,
                        SpielProvider *other,
                        gpointer user_data)
{
  return g_strcmp0 (spiel_provider_get_well_known_name (self),
                    spiel_provider_get_well_known_name (other));
}

static void
spiel_provider_finalize (GObject *object)
{
  SpielProvider *self = (SpielProvider *) object;
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);

  g_signal_handler_disconnect (priv->provider_proxy,
                               priv->voices_changed_handler_id);

  g_clear_object (&(priv->provider_proxy));
  g_clear_object (&(priv->voices));

  G_OBJECT_CLASS (spiel_provider_parent_class)->finalize (object);
}

static void
spiel_provider_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
  SpielProvider *self = SPIEL_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, spiel_provider_get_name (self));
      break;
    case PROP_WELL_KNOWN_NAME:
      g_value_set_string (value, spiel_provider_get_well_known_name (self));
      break;
    case PROP_VOICES:
      g_value_set_object (value, spiel_provider_get_voices (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spiel_provider_class_init (SpielProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_provider_finalize;
  object_class->get_property = spiel_provider_get_property;

  /**
   * SpielProvider:name: (getter get_name)
   *
   * The provider's human readable name
   *
   */
  properties[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL, NULL /* default value */,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielProvider:well-known-name: (getter get_well_known_name)
   *
   * The provider's D-Bus well known name.
   *
   */
  properties[PROP_WELL_KNOWN_NAME] = g_param_spec_string (
      "well-known-name", NULL, NULL, NULL /* default value */,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielProvider:voices:
   *
   * The list of available [class@Voice]s that are provided.
   *
   */
  properties[PROP_VOICES] =
      g_param_spec_object ("voices", NULL, NULL, G_TYPE_LIST_MODEL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
spiel_provider_init (SpielProvider *self)
{
  SpielProviderPrivate *priv = spiel_provider_get_instance_private (self);
  priv->provider_proxy = NULL;
  priv->is_activatable = FALSE;
  priv->voices = g_list_store_new (SPIEL_TYPE_VOICE);
  priv->voices_hashset = g_hash_table_new ((GHashFunc) spiel_voice_hash,
                                           (GCompareFunc) spiel_voice_equal);
}
