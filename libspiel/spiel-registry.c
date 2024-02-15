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
#include "spiel-provider-private.h"
#include "spiel-registry.h"
#include "spiel-voices-list-model.h"

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
  GListStore *providers;
  SpielVoicesListModel *voices;
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

static SpielProvider *
_get_provider_by_name (GListStore *providers,
                       const char *provider_name,
                       guint *position)
{
  guint providers_count = g_list_model_get_n_items (G_LIST_MODEL (providers));

  for (guint i = 0; i < providers_count; i++)
    {
      SpielProvider *provider = SPIEL_PROVIDER (
          g_list_model_get_object (G_LIST_MODEL (providers), i));
      g_object_unref (provider); // Just want to borrow a ref.
      if (g_str_equal (provider_name,
                       spiel_provider_get_well_known_name (provider)))
        {
          if (position)
            {
              *position = i;
            }
          return provider;
        }
    }

  return NULL;
}

static void
_insert_providers (const char *provider_name,
                   SpielProvider *new_provider,
                   SpielRegistry *self)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  SpielProvider *provider =
      _get_provider_by_name (priv->providers, provider_name, NULL);

  if (!provider)
    {
      g_list_store_insert_sorted (priv->providers, new_provider,
                                  (GCompareDataFunc) spiel_provider_compare,
                                  NULL);
    }
  else
    {
      spiel_provider_set_is_activatable (
          provider, spiel_provider_get_is_activatable (new_provider));
    }
}

static void
_on_providers_updated (GObject *source, GAsyncResult *res, SpielRegistry *self)
{
  GError *err = NULL;
  GHashTable *new_providers = spiel_collect_providers_finish (res, &err);
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  guint providers_count = 0;

  if (err != NULL)
    {
      g_warning ("Error updating providers: %s\n", err->message);
      g_error_free (err);
      return;
    }

  g_hash_table_foreach (new_providers, (GHFunc) _insert_providers, self);

  providers_count = g_list_model_get_n_items (G_LIST_MODEL (priv->providers));

  for (gint i = providers_count - 1; i >= 0; i--)
    {
      SpielProvider *provider = SPIEL_PROVIDER (
          g_list_model_get_object (G_LIST_MODEL (priv->providers), i));
      g_object_unref (provider); // Just want to borrow a ref.
      if (!g_hash_table_contains (
              new_providers, spiel_provider_get_well_known_name (provider)))
        {
          g_list_store_remove (priv->providers, i);
        }
    }

  g_hash_table_unref (new_providers);
}

static void
_on_new_provider_collected (GObject *source,
                            GAsyncResult *res,
                            SpielRegistry *self)
{
  GError *err = NULL;
  SpielProvider *provider = spiel_collect_provider_finish (res, &err);
  const char *provider_name;

  if (err != NULL)
    {
      g_warning ("Error collecting provider: %s\n", err->message);
      g_error_free (err);
      return;
    }

  provider_name = spiel_provider_get_well_known_name (provider);
  _insert_providers (provider_name, provider, self);

  g_object_unref (provider);
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
  spiel_collect_providers (connection, NULL,
                           (GAsyncReadyCallback) _on_providers_updated, self);
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
      guint position = 0;
      SpielProvider *provider =
          _get_provider_by_name (priv->providers, service_name, &position);
      if (provider_removed)
        {
          if (provider && !spiel_provider_get_is_activatable (provider))
            {
              g_list_store_remove (priv->providers, position);
            }

          g_signal_emit (self, registry_signals[PROVIDER_DIED], 0,
                         service_name);
        }
      else if (!provider)
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
_on_providers_collected (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GTask *task = user_data;
  GError *err = NULL;
  SpielRegistry *self = g_task_get_source_object (task);
  GHashTable *providers = spiel_collect_providers_finish (res, &err);

  if (err != NULL)
    {
      g_warning ("Error retrieving providers: %s\n", err->message);
      g_task_return_error (task, err);
      return;
    }

  g_hash_table_foreach (providers, (GHFunc) _insert_providers, self);

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

  spiel_collect_providers (bus, cancellable, _on_providers_collected, task);
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

  priv->providers = g_list_store_new (SPIEL_TYPE_PROVIDER);
  priv->voices = spiel_voices_list_model_new (G_LIST_MODEL (priv->providers));
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

  g_clear_object (&priv->providers);
  g_clear_object (&priv->voices);
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
  GHashTable *providers = NULL;

  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  providers = spiel_collect_providers_sync (bus, cancellable, error);

  if (*error != NULL)
    {
      g_warning ("Error retrieving providers: %s\n", (*error)->message);
      return FALSE;
    }

  if (providers)
    {
      g_hash_table_foreach (providers, (GHFunc) _insert_providers, self);
    }

  priv->connection = g_object_ref (bus);

  _subscribe_to_activatable_services_changed (self);

  return TRUE;
}

static void
spiel_registry_init (SpielRegistry *self)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  priv->providers = g_list_store_new (SPIEL_TYPE_PROVIDER);
  priv->voices = spiel_voices_list_model_new (G_LIST_MODEL (priv->providers));
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

/* Public API */

SpielProviderProxy *
spiel_registry_get_provider_for_voice (SpielRegistry *self, SpielVoice *voice)
{
  SpielProvider *provider = spiel_voice_get_provider (voice);

  g_return_val_if_fail (provider, NULL);

  return spiel_provider_get_proxy (provider);
}

static SpielVoice *
_get_voice_from_provider_and_name (SpielRegistry *self,
                                   const char *provider_name,
                                   const char *voice_id)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  SpielProvider *provider =
      _get_provider_by_name (priv->providers, provider_name, NULL);
  g_return_val_if_fail (provider, NULL);

  return spiel_provider_get_voice_by_id (provider, voice_id);
}

static SpielVoice *
_get_fallback_voice (SpielRegistry *self, const char *language)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);

  if (language)
    {
      guint voices_count =
          g_list_model_get_n_items (G_LIST_MODEL (priv->voices));

      for (guint i = 0; i < voices_count; i++)
        {
          SpielVoice *voice = SPIEL_VOICE (
              g_list_model_get_object (G_LIST_MODEL (priv->voices), i));
          if (g_strv_contains (spiel_voice_get_languages (voice), language))
            {
              return voice;
            }
          g_object_unref (voice); // Just want to borrow a ref.
        }
    }

  return g_list_model_get_item ((GListModel *) priv->voices, 0);
  ;
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

GListModel *
spiel_registry_get_voices (SpielRegistry *self)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  return G_LIST_MODEL (priv->voices);
}

GListModel *
spiel_registry_get_providers (SpielRegistry *self)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  return G_LIST_MODEL (priv->providers);
}
