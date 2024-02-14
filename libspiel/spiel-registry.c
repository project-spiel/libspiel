/* spiel-registry.c
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
#include "spiel-registry.h"

#include <gio/gio.h>
#include <gst/gst.h>

#define GSETTINGS_SCHEMA "org.monotonous.libspiel"

struct _SpielRegistry
{
  GObject parent_instance;
};

typedef struct
{
  GDBusConnection *connection;
  guint subscription_ids[2];
  GHashTable *providers;
  GListStore *voices;
  GSettings *settings;
} SpielRegistryPrivate;

static void initable_iface_init (GInitableIface *initable_iface);
static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SpielRegistry,
    spiel_registry,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (SpielRegistry)
        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
            G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                   async_initable_iface_init))

static SpielRegistry *sRegistry = NULL;

enum
{
  PROVIDER_DIED,
  LAST_SIGNAL
};

static guint registry_signals[LAST_SIGNAL] = { 0 };

typedef struct
{
  SpielProviderProxy *provider;
  GHashTable *voices_hashset;
  gboolean is_activatable;
  gulong voices_changed_handler_id;
} _ProviderEntry;

static gboolean handle_voices_changed (SpielProviderProxy *provider,
                                       GParamSpec *spec,
                                       gpointer user_data);

static void
_provider_entry_destroy (gpointer data)
{
  _ProviderEntry *entry = data;
  g_signal_handler_disconnect (entry->provider,
                               entry->voices_changed_handler_id);
  g_clear_object (&entry->provider);
  g_hash_table_unref (entry->voices_hashset);
  entry->voices_hashset = NULL;
  entry->voices_changed_handler_id = 0;

  g_slice_free (_ProviderEntry, entry);
}

static void
_update_voices (SpielRegistry *self,
                GSList *new_voices,
                GHashTable *voices_hashset)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  GHashTable *new_voices_hashset = NULL;

  if (g_hash_table_size (voices_hashset) > 0)
    {
      // We are adding voices to an already populated provider, store
      // new voices in a hashset for easy purge of ones that were removed.
      new_voices_hashset = g_hash_table_new ((GHashFunc) spiel_voice_hash,
                                             (GCompareFunc) spiel_voice_equal);
    }

  if (new_voices)
    {
      for (GSList *item = new_voices; item; item = item->next)
        {
          SpielVoice *voice = item->data;
          if (!g_hash_table_contains (voices_hashset, voice))
            {
              g_hash_table_insert (voices_hashset, g_object_ref (voice), NULL);
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
      g_hash_table_iter_init (&voices_iter, voices_hashset);
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
}

static void
_insert_providers_and_voices (const char *provider_name,
                              ProviderAndVoices *provider_and_voices,
                              SpielRegistry *self)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  _ProviderEntry *provider_entry =
      g_hash_table_lookup (priv->providers, provider_name);

  if (!provider_entry)
    {
      provider_entry = g_slice_new0 (_ProviderEntry);
      provider_entry->provider = g_object_ref (provider_and_voices->provider);
      provider_entry->voices_hashset = g_hash_table_new_full (
          (GHashFunc) spiel_voice_hash, (GCompareFunc) spiel_voice_equal,
          g_object_unref, NULL);
      provider_entry->voices_changed_handler_id =
          g_signal_connect (provider_entry->provider, "notify::voices",
                            G_CALLBACK (handle_voices_changed), self);
      g_hash_table_insert (priv->providers, g_strdup (provider_name),
                           provider_entry);
    }

  provider_entry->is_activatable = provider_and_voices->is_activatable;

  _update_voices (self, provider_and_voices->voices,
                  provider_entry->voices_hashset);
}

static void
_on_providers_and_voices_updated (GObject *source,
                                  GAsyncResult *res,
                                  SpielRegistry *self)
{
  GError *err = NULL;
  GHashTable *providers_and_voices = spiel_collect_providers_finish (res, &err);
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  GHashTableIter current_providers_iter;
  const char *provider_name;
  _ProviderEntry *provider_entry;

  if (err != NULL)
    {
      g_warning ("Error updating providers: %s\n", err->message);
      g_error_free (err);
      return;
    }

  g_hash_table_foreach (providers_and_voices,
                        (GHFunc) _insert_providers_and_voices, self);

  g_hash_table_iter_init (&current_providers_iter, priv->providers);
  while (g_hash_table_iter_next (&current_providers_iter,
                                 (gpointer *) &provider_name,
                                 (gpointer *) &provider_entry))
    {
      if (!g_hash_table_contains (providers_and_voices, provider_name))
        {
          _update_voices (self, NULL, provider_entry->voices_hashset);
          g_hash_table_iter_remove (&current_providers_iter);
        }
    }

  g_hash_table_unref (providers_and_voices);
}

static void
_on_new_provider_collected (GObject *source,
                            GAsyncResult *res,
                            SpielRegistry *self)
{
  GError *err = NULL;
  ProviderAndVoices *provider_and_voices =
      spiel_collect_provider_finish (res, &err);
  const char *provider_name;

  if (err != NULL)
    {
      g_warning ("Error collecting provider: %s\n", err->message);
      g_error_free (err);
      return;
    }

  provider_name =
      g_dbus_proxy_get_name (G_DBUS_PROXY (provider_and_voices->provider));
  _insert_providers_and_voices (provider_name, provider_and_voices, self);

  spiel_collect_free_provider_and_voices (provider_and_voices);
}

static void
_maybe_activatable_providers_changed (GDBusConnection *connection,
                                      const gchar *sender_name,
                                      const gchar *object_path,
                                      const gchar *interface_name,
                                      const gchar *signal_name,
                                      GVariant *parameters,
                                      gpointer user_data)
{
  SpielRegistry *self = user_data;

  // No arguments given, so update the whole providers cache.
  spiel_collect_providers (
      connection, NULL, (GAsyncReadyCallback) _on_providers_and_voices_updated,
      self);
}

static void
_maybe_running_providers_changed (GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data)
{
  SpielRegistry *self = user_data;
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  const char *service_name;
  const char *old_owner;
  const char *new_owner;
  g_variant_get (parameters, "(&s&s&s)", &service_name, &old_owner, &new_owner);
  if (g_str_has_suffix (service_name, PROVIDER_SUFFIX))
    {
      gboolean provider_removed = strlen (new_owner) == 0;

      if (provider_removed)
        {
          _ProviderEntry *entry =
              g_hash_table_lookup (priv->providers, service_name);

          if (entry && !entry->is_activatable)
            {
              _update_voices (self, NULL, entry->voices_hashset);
              g_hash_table_remove (priv->providers, service_name);
            }

          g_signal_emit (self, registry_signals[PROVIDER_DIED], 0,
                         service_name);
        }
      else if (!g_hash_table_contains (priv->providers, service_name))
        {
          spiel_collect_provider (
              connection, NULL, service_name,
              (GAsyncReadyCallback) _on_new_provider_collected, self);
        }
    }
}

static void
_subscribe_to_activatable_services_changed (SpielRegistry *self)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  priv->subscription_ids[0] = g_dbus_connection_signal_subscribe (
      priv->connection, "org.freedesktop.DBus", "org.freedesktop.DBus",
      "ActivatableServicesChanged", "/org/freedesktop/DBus", NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, _maybe_activatable_providers_changed, self,
      NULL);

  priv->subscription_ids[1] = g_dbus_connection_signal_subscribe (
      priv->connection, "org.freedesktop.DBus", "org.freedesktop.DBus",
      "NameOwnerChanged", "/org/freedesktop/DBus", NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, _maybe_running_providers_changed,
      g_object_ref (self), g_object_unref);
}

static void
_on_providers_and_voices_collected (GObject *source,
                                    GAsyncResult *res,
                                    gpointer user_data)
{
  GTask *task = user_data;
  GError *err = NULL;
  SpielRegistry *self = g_task_get_source_object (task);
  GHashTable *providers_and_voices = spiel_collect_providers_finish (res, &err);

  if (err != NULL)
    {
      g_warning ("Error retrieving providers: %s\n", err->message);
      g_task_return_error (task, err);
      return;
    }

  g_hash_table_foreach (providers_and_voices,
                        (GHFunc) _insert_providers_and_voices, self);

  _subscribe_to_activatable_services_changed (self);

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static void
_on_bus_get (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  GCancellable *cancellable = g_task_get_task_data (task);
  SpielRegistry *self = g_task_get_source_object (task);
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);

  GError *error = NULL;
  GDBusConnection *bus = g_bus_get_finish (result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  priv->connection = g_object_ref (bus);

  spiel_collect_providers (bus, cancellable, _on_providers_and_voices_collected,
                           task);
}

void
spiel_registry_get (GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
  if (sRegistry != NULL)
    {
      GTask *task = g_task_new (g_object_ref (sRegistry), cancellable, callback,
                                user_data);
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }
  else
    {
      g_async_initable_new_async (SPIEL_TYPE_REGISTRY, G_PRIORITY_DEFAULT,
                                  cancellable, callback, user_data, NULL);
    }
}

SpielRegistry *
spiel_registry_get_finish (GAsyncResult *result, GError **error)
{
  GObject *object;
  GObject *source_object;

  source_object = g_async_result_get_source_object (result);
  g_assert (source_object != NULL);

  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        result, error);
  g_object_unref (source_object);

  if (object != NULL)
    {
      if (sRegistry == NULL)
        {
          gst_init_check (NULL, NULL, error);
          sRegistry = SPIEL_REGISTRY (object);
        }
      g_assert (sRegistry == SPIEL_REGISTRY (object));
      return SPIEL_REGISTRY (object);
    }
  else
    {
      return NULL;
    }
}

SpielRegistry *
spiel_registry_get_sync (GCancellable *cancellable, GError **error)
{
  if (sRegistry == NULL)
    {
      gst_init_check (NULL, NULL, error);
      sRegistry =
          g_initable_new (SPIEL_TYPE_REGISTRY, cancellable, error, NULL);
    }
  else
    {
      g_object_ref (sRegistry);
    }

  return sRegistry;
}

static GSettings *
_settings_new (void)
{
  GSettingsSchema *schema = NULL;
  GSettingsSchemaSource *source = g_settings_schema_source_get_default ();
  GSettings *settings = NULL;

  schema = g_settings_schema_source_lookup (source, GSETTINGS_SCHEMA, TRUE);
  if (!schema)
    {
      g_warning ("libspiel settings schema is not installed");
      return NULL;
    }

  settings = g_settings_new (GSETTINGS_SCHEMA);
  g_settings_schema_unref (schema);

  return settings;
}

static void
async_initable_init_async (GAsyncInitable *initable,
                           gint io_priority,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
  GTask *task = g_task_new (initable, cancellable, callback, user_data);
  SpielRegistry *self = SPIEL_REGISTRY (initable);
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);

  priv->providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                           _provider_entry_destroy);
  priv->voices = g_list_store_new (SPIEL_TYPE_VOICE);
  priv->settings = _settings_new ();

  if (cancellable != NULL)
    {
      g_task_set_task_data (task, g_object_ref (cancellable), g_object_unref);
    }
  g_bus_get (G_BUS_TYPE_SESSION, cancellable, _on_bus_get, task);
}

static gboolean
async_initable_init_finish (GAsyncInitable *initable,
                            GAsyncResult *res,
                            GError **error)
{
  g_return_val_if_fail (g_task_is_valid (res, initable), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

static void
spiel_registry_finalize (GObject *object)
{
  SpielRegistry *self = (SpielRegistry *) object;
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);

  g_hash_table_unref (priv->providers);
  g_clear_object (&priv->settings);
  if (priv->connection)
    {
      g_dbus_connection_signal_unsubscribe (priv->connection,
                                            priv->subscription_ids[0]);
      g_dbus_connection_signal_unsubscribe (priv->connection,
                                            priv->subscription_ids[1]);
      g_clear_object (&priv->connection);
    }

  G_OBJECT_CLASS (spiel_registry_parent_class)->finalize (object);
  g_assert (object == G_OBJECT (sRegistry));
  sRegistry = NULL;
}

static gboolean
initable_init (GInitable *initable, GCancellable *cancellable, GError **error)
{
  SpielRegistry *self = SPIEL_REGISTRY (initable);
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  GHashTable *providers_and_voices = NULL;

  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  providers_and_voices = spiel_collect_providers_sync (bus, cancellable, error);

  if (*error != NULL)
    {
      g_warning ("Error retrieving providers: %s\n", (*error)->message);
      return FALSE;
    }

  if (providers_and_voices)
    {
      g_hash_table_foreach (providers_and_voices,
                            (GHFunc) _insert_providers_and_voices, self);
    }

  priv->connection = g_object_ref (bus);

  _subscribe_to_activatable_services_changed (self);

  return TRUE;
}

static void
spiel_registry_init (SpielRegistry *self)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  priv->providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                           _provider_entry_destroy);
  priv->voices = g_list_store_new (SPIEL_TYPE_VOICE);
  priv->settings = _settings_new ();
}

static void
spiel_registry_class_init (SpielRegistryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_registry_finalize;

  registry_signals[PROVIDER_DIED] = g_signal_new (
      "provider-died", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface)
{
  async_initable_iface->init_async = async_initable_init_async;
  async_initable_iface->init_finish = async_initable_init_finish;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

/* Signal handlers */

static gboolean
handle_voices_changed (SpielProviderProxy *provider,
                       GParamSpec *spec,
                       gpointer user_data)
{
  SpielRegistry *self = user_data;
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  const char *provider_name = g_dbus_proxy_get_name (G_DBUS_PROXY (provider));
  _ProviderEntry *provider_entry =
      g_hash_table_lookup (priv->providers, provider_name);
  char *name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (provider));
  GSList *changed_voices = NULL;

  if (name_owner == NULL && provider_entry->is_activatable)
    {
      // Got a change notification because an activatable service left the bus.
      // Its voices are still valid, though.
      return TRUE;
    }

  g_free (name_owner);

  changed_voices = spiel_collect_provider_voices (provider);
  provider_entry = g_hash_table_lookup (priv->providers, provider_name);
  _update_voices (self, changed_voices, provider_entry->voices_hashset);

  g_slist_free_full (changed_voices, (GDestroyNotify) g_object_unref);

  return TRUE;
}

/* Public API */

SpielProviderProxy *
spiel_registry_get_provider_for_voice (SpielRegistry *self, SpielVoice *voice)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);

  _ProviderEntry *provider_entry = g_hash_table_lookup (
      priv->providers, spiel_voice_get_provider_name (voice));

  if (!provider_entry)
    {
      g_warning ("No provider for voice");
      return NULL;
    }

  return provider_entry->provider;
}

static SpielVoice *
_get_voice_from_provider_and_name (SpielRegistry *self,
                                   const char *provider_name,
                                   const char *voice_id)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  _ProviderEntry *provider_entry =
      g_hash_table_lookup (priv->providers, provider_name);
  SpielVoice **voices = NULL;
  SpielVoice *voice = NULL;
  if (provider_entry == NULL)
    {
      return NULL;
    }

  voices = (SpielVoice **) g_hash_table_get_keys_as_array (
      provider_entry->voices_hashset, NULL);
  for (SpielVoice **v = voices; *v; v++)
    {
      if (g_str_equal (spiel_voice_get_identifier (*v), voice_id))
        {
          voice = *v;
        }
    }

  return voice;
}

static gboolean
_match_voice_with_language (SpielVoice *voice,
                            gconstpointer unused,
                            const char *language)
{
  return g_strv_contains (spiel_voice_get_languages (voice), language);
}

static SpielVoice *
_get_fallback_voice (SpielRegistry *self, const char *language)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  SpielVoice *voice = NULL;
  guint position = 0;

  if (language)
    {
      g_list_store_find_with_equal_func_full (
          priv->voices, NULL, (GEqualFuncFull) _match_voice_with_language,
          (gpointer) language, &position);
    }

  voice = g_list_model_get_item ((GListModel *) priv->voices, position);

  return voice;
}

SpielVoice *
spiel_registry_get_voice_for_utterance (SpielRegistry *self,
                                        SpielUtterance *utterance)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  char *provider_name = NULL;
  char *voice_id = NULL;
  const char *language = spiel_utterance_get_language (utterance);

  SpielVoice *voice = spiel_utterance_get_voice (utterance);
  if (voice)
    {
      return voice;
    }

  if (language && priv->settings)
    {
      GVariant *mapping =
          g_settings_get_value (priv->settings, "language-voice-mapping");
      char *_lang = g_strdup (language);
      char *found = _lang + g_utf8_strlen (_lang, -1);
      gssize boundary = -1;

      do
        {
          *found = 0;
          g_variant_lookup (mapping, _lang, "(ss)", &provider_name, &voice_id);
          if (provider_name)
            {
              break;
            }
          found = g_utf8_strrchr (_lang, boundary, '-');
          boundary = found - _lang - 1;
        }
      while (found);

      g_free (_lang);
    }

  if (!provider_name && priv->settings)
    {
      g_settings_get (priv->settings, "default-voice", "m(ss)", NULL,
                      &provider_name, &voice_id);
    }

  if (provider_name)
    {
      g_assert (voice_id != NULL);
      voice = _get_voice_from_provider_and_name (self, provider_name, voice_id);
      g_free (provider_name);
      g_free (voice_id);
    }

  if (voice)
    {
      return voice;
    }

  return _get_fallback_voice (self, language);
}

GListStore *
spiel_registry_get_voices (SpielRegistry *self)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  return priv->voices;
}