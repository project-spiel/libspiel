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
 * Since: 1.0
 */

struct _SpielProvider
{
  GObject parent_instance;
  SpielProviderProxy *provider_proxy;
  gboolean is_activatable;
  GListStore *voices;
  GHashTable *voices_hashset;
  gulong voices_changed_handler_id;
};

G_DEFINE_FINAL_TYPE (SpielProvider, spiel_provider, G_TYPE_OBJECT)

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

/*< private >
 * spiel_provider_new: (constructor)
 *
 * Creates a new [class@Spiel.Provider].
 *
 * Returns: (transfer full): The new `SpielProvider`.
 */
SpielProvider *
spiel_provider_new (void)
{
  return g_object_new (SPIEL_TYPE_PROVIDER, NULL);
}

/*< private >
 * spiel_provider_set_proxy:
 * @self: a `SpielProvider`
 * @provider_proxy: a `SpielProviderProxy`
 *
 * Sets the internal D-Bus proxy.
 */
void
spiel_provider_set_proxy (SpielProvider *self,
                          SpielProviderProxy *provider_proxy)
{
  g_return_if_fail (SPIEL_IS_PROVIDER (self));
  g_assert (!self->provider_proxy);

  self->provider_proxy = g_object_ref (provider_proxy);

  _spiel_provider_update_voices (self);

  self->voices_changed_handler_id =
      g_signal_connect (self->provider_proxy, "notify::voices",
                        G_CALLBACK (handle_voices_changed), self);
}

/*< private >
 * spiel_provider_get_proxy:
 * @self: a `SpielProvider`
 *
 * Gets the internal D-Bus proxy.
 *
 * Returns: (transfer none): a `SpielProviderProxy`
 */
SpielProviderProxy *
spiel_provider_get_proxy (SpielProvider *self)
{
  g_return_val_if_fail (SPIEL_IS_PROVIDER (self), NULL);

  return self->provider_proxy;
}

/*< private >
 * spiel_provider_get_voice_by_id:
 * @self: a `SpielProvider`
 * @voice_id: (not nullable): a voice ID
 *
 * Lookup a `SpielVoice` by ID.
 *
 * Returns: (transfer none) (nullable): a `SpielProviderProxy`
 */
SpielVoice *
spiel_provider_get_voice_by_id (SpielProvider *self, const char *voice_id)
{
  guint voices_count = 0;

  g_return_val_if_fail (SPIEL_IS_PROVIDER (self), NULL);
  g_return_val_if_fail (voice_id != NULL, NULL);

  voices_count = g_list_model_get_n_items (G_LIST_MODEL (self->voices));

  for (guint i = 0; i < voices_count; i++)
    {
      g_autoptr (SpielVoice) voice = SPIEL_VOICE (
          g_list_model_get_object (G_LIST_MODEL (self->voices), i));
      if (g_str_equal (spiel_voice_get_identifier (voice), voice_id))
        {
          return g_steal_pointer (&voice);
        }
    }
  return NULL;
}

/**
 * spiel_provider_get_name: (get-property name)
 * @self: a `SpielProvider`
 *
 * Gets a human readable name of this provider
 *
 * Returns: (transfer none): the human readable name.
 *
 * Since: 1.0
 */
const char *
spiel_provider_get_name (SpielProvider *self)
{
  g_return_val_if_fail (SPIEL_IS_PROVIDER (self), NULL);
  g_return_val_if_fail (self->provider_proxy, NULL);

  return spiel_provider_proxy_get_name (self->provider_proxy);
}

/**
 * spiel_provider_get_well_known_name: (get-property well-known-name)
 * @self: a `SpielProvider`
 *
 * Gets the provider's D-Bus well known name.
 *
 * Returns: (transfer none): the well known name.
 *
 * Since: 1.0
 */
const char *
spiel_provider_get_well_known_name (SpielProvider *self)
{
  g_return_val_if_fail (SPIEL_IS_PROVIDER (self), NULL);
  g_return_val_if_fail (self->provider_proxy, NULL);

  return g_dbus_proxy_get_name (G_DBUS_PROXY (self->provider_proxy));
}

/**
 * spiel_provider_get_voices: (get-property voices)
 * @self: a `SpielProvider`
 *
 * Gets the provider's voices.
 *
 * Returns: (transfer none): A list of available voices
 *
 * Since: 1.0
 */
GListModel *
spiel_provider_get_voices (SpielProvider *self)
{
  g_return_val_if_fail (SPIEL_IS_PROVIDER (self), NULL);

  return G_LIST_MODEL (self->voices);
}

/*< private >
 * spiel_provider_set_is_activatable:
 * @self: a `SpielProvider`
 * @is_activatable: %TRUE if activatable
 *
 * Sets whether the provider supports D-Bus activation.
 */
void
spiel_provider_set_is_activatable (SpielProvider *self, gboolean is_activatable)
{
  g_return_if_fail (SPIEL_IS_PROVIDER (self));

  self->is_activatable = is_activatable;
}

/*< private >
 * spiel_provider_get_is_activatable:
 * @self: a `SpielProvider`
 *
 * Gets whether the provider supports D-Bus activation.
 *
 * Returns: %TRUE if activatable
 */
gboolean
spiel_provider_get_is_activatable (SpielProvider *self)
{
  g_return_val_if_fail (SPIEL_IS_PROVIDER (self), FALSE);

  return self->is_activatable;
}

static GSList *
_create_provider_voices (SpielProvider *self)
{
  GSList *voices_slist = NULL;
  GVariant *voices = spiel_provider_proxy_get_voices (self->provider_proxy);
  gsize voices_count = voices ? g_variant_n_children (voices) : 0;

  for (gsize i = 0; i < voices_count; i++)
    {
      const char *name = NULL;
      const char *identifier = NULL;
      const char *output_format = NULL;
      g_autofree char **languages = NULL;
      guint64 features;
      SpielVoice *voice;

      g_variant_get_child (voices, i, "(&s&s&st^a&s)", &name, &identifier,
                           &output_format, &features, &languages);
      if (features >> 32)
        {
          g_warning ("Voice features past 32 bits are ignored in %s (%s)",
                     identifier, spiel_provider_get_well_known_name (self));
        }
      voice = g_object_new (SPIEL_TYPE_VOICE, "name", name, "identifier",
                            identifier, "languages", languages, "provider",
                            self, "features", features, NULL);
      spiel_voice_set_output_format (voice, output_format);

      voices_slist = g_slist_prepend (voices_slist, voice);
    }

  return voices_slist;
}

static void
_spiel_provider_update_voices (SpielProvider *self)
{
  GSList *new_voices = NULL;
  g_autoptr (GHashTable) new_voices_hashset = NULL;

  g_return_if_fail (self->provider_proxy);

  new_voices = _create_provider_voices (self);
  if (g_hash_table_size (self->voices_hashset) > 0)
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
          if (!g_hash_table_contains (self->voices_hashset, voice))
            {
              g_hash_table_insert (self->voices_hashset, g_object_ref (voice),
                                   NULL);
              g_list_store_insert_sorted (
                  self->voices, voice, (GCompareDataFunc) spiel_voice_compare,
                  NULL);
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
      g_hash_table_iter_init (&voices_iter, self->voices_hashset);
      while (
          g_hash_table_iter_next (&voices_iter, (gpointer *) &old_voice, NULL))
        {
          if (!g_hash_table_contains (new_voices_hashset, old_voice))
            {
              guint position = 0;
              if (g_list_store_find (self->voices, old_voice, &position))
                {
                  g_list_store_remove (self->voices, position);
                }
              g_hash_table_iter_remove (&voices_iter);
            }
        }
    }

  g_slist_free_full (new_voices, (GDestroyNotify) g_object_unref);
}

static gboolean
handle_voices_changed (SpielProviderProxy *provider_proxy,
                       GParamSpec *spec,
                       gpointer user_data)
{
  SpielProvider *self = user_data;
  g_autofree char *name_owner =
      g_dbus_proxy_get_name_owner (G_DBUS_PROXY (self->provider_proxy));

  if (name_owner == NULL && self->is_activatable)
    {
      // Got a change notification because an activatable service left the bus.
      // Its voices are still valid, though.
      return TRUE;
    }

  _spiel_provider_update_voices (self);

  return TRUE;
}

/*< private >
 * spiel_provider_compare:
 * @self: (not nullable): a `SpielProvider`
 * @other: (not nullable): a `SpielProvider` to compare with @self
 * @user_data: user-defined callback data
 *
 * Compares the two [class@Spiel.Provider] values and returns a negative integer
 * if the first value comes before the second, 0 if they are equal, or a
 * positive integer if the first value comes after the second.
 *
 * Returns: an integer indicating order
 */
gint
spiel_provider_compare (SpielProvider *self,
                        SpielProvider *other,
                        gpointer user_data)
{
  g_return_val_if_fail (SPIEL_IS_PROVIDER (self), 0);
  g_return_val_if_fail (SPIEL_IS_PROVIDER (other), 0);

  return g_strcmp0 (spiel_provider_get_well_known_name (self),
                    spiel_provider_get_well_known_name (other));
}

static void
spiel_provider_finalize (GObject *object)
{
  SpielProvider *self = (SpielProvider *) object;

  g_signal_handler_disconnect (self->provider_proxy,
                               self->voices_changed_handler_id);

  g_clear_object (&(self->provider_proxy));
  g_clear_object (&(self->voices));

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
   * Since: 1.0
   */
  properties[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL, NULL /* default value */,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielProvider:well-known-name: (getter get_well_known_name)
   *
   * The provider's D-Bus well known name.
   *
   * Since: 1.0
   */
  properties[PROP_WELL_KNOWN_NAME] = g_param_spec_string (
      "well-known-name", NULL, NULL, NULL /* default value */,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielProvider:voices: (getter get_voices)
   *
   * The list of available [class@Spiel.Voice]s that are provided.
   *
   * Since: 1.0
   */
  properties[PROP_VOICES] =
      g_param_spec_object ("voices", NULL, NULL, G_TYPE_LIST_MODEL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
spiel_provider_init (SpielProvider *self)
{
  self->provider_proxy = NULL;
  self->is_activatable = FALSE;
  self->voices = g_list_store_new (SPIEL_TYPE_VOICE);
  self->voices_hashset = g_hash_table_new ((GHashFunc) spiel_voice_hash,
                                           (GCompareFunc) spiel_voice_equal);
}
