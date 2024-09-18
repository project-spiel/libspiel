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
  GDBusConnection *connection;
  guint subscription_ids[2];
  GListStore *providers;
  SpielVoicesListModel *voices;
  GSettings *settings;
};

static void initable_iface_init (GInitableIface *initable_iface);
static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SpielRegistry,
    spiel_registry,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                               async_initable_iface_init))

static SpielRegistry *sRegistry = NULL;
static GSList *sPendingTasks = NULL;

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
      g_autoptr (SpielProvider) provider = SPIEL_PROVIDER (
          g_list_model_get_object (G_LIST_MODEL (providers), i));
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
  SpielProvider *provider =
      _get_provider_by_name (self->providers, provider_name, NULL);

  if (!provider)
    {
      g_list_store_insert_sorted (self->providers, new_provider,
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
  g_autoptr (GError) err = NULL;
  g_autoptr (GHashTable) new_providers =
      spiel_collect_providers_finish (res, &err);
  guint providers_count = 0;

  if (err != NULL)
    {
      g_warning ("Error updating providers: %s\n", err->message);
      return;
    }

  g_hash_table_foreach (new_providers, (GHFunc) _insert_providers, self);

  providers_count = g_list_model_get_n_items (G_LIST_MODEL (self->providers));

  for (gint i = providers_count - 1; i >= 0; i--)
    {
      g_autoptr (SpielProvider) provider = SPIEL_PROVIDER (
          g_list_model_get_object (G_LIST_MODEL (self->providers), i));
      if (!g_hash_table_contains (
              new_providers, spiel_provider_get_well_known_name (provider)))
        {
          g_list_store_remove (self->providers, i);
        }
    }
}

static void
_on_new_provider_collected (GObject *source,
                            GAsyncResult *res,
                            SpielRegistry *self)
{
  g_autoptr (GError) err = NULL;
  g_autoptr (SpielProvider) provider =
      spiel_collect_provider_finish (res, &err);
  const char *provider_name;

  if (err != NULL)
    {
      g_warning ("Error collecting provider: %s\n", err->message);
      return;
    }

  provider_name = spiel_provider_get_well_known_name (provider);
  _insert_providers (provider_name, provider, self);
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
  const char *service_name;
  const char *old_owner;
  const char *new_owner;
  g_variant_get (parameters, "(&s&s&s)", &service_name, &old_owner, &new_owner);
  if (g_str_has_suffix (service_name, PROVIDER_SUFFIX))
    {
      gboolean provider_removed = strlen (new_owner) == 0;
      guint position = 0;
      SpielProvider *provider =
          _get_provider_by_name (self->providers, service_name, &position);
      if (provider_removed)
        {
          if (provider && !spiel_provider_get_is_activatable (provider))
            {
              g_list_store_remove (self->providers, position);
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
  self->subscription_ids[0] = g_dbus_connection_signal_subscribe (
      self->connection, "org.freedesktop.DBus", "org.freedesktop.DBus",
      "ActivatableServicesChanged", "/org/freedesktop/DBus", NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, _maybe_activatable_providers_changed, self,
      NULL);

  self->subscription_ids[1] = g_dbus_connection_signal_subscribe (
      self->connection, "org.freedesktop.DBus", "org.freedesktop.DBus",
      "NameOwnerChanged", "/org/freedesktop/DBus", NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, _maybe_running_providers_changed,
      g_object_ref (self), g_object_unref);
}

static void
_on_providers_collected (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GTask *top_task = user_data;
  g_autoptr (GError) err = NULL;
  SpielRegistry *self = g_task_get_source_object (top_task);
  g_autoptr (GHashTable) providers = spiel_collect_providers_finish (res, &err);
  g_assert (sPendingTasks->data == top_task);

  if (err != NULL)
    {
      g_warning ("Error retrieving providers: %s\n", err->message);
      while (sPendingTasks)
        {
          GTask *task = sPendingTasks->data;
          g_task_return_error (task, g_error_copy (err));
          g_object_unref (task);
          sPendingTasks = g_slist_delete_link (sPendingTasks, sPendingTasks);
        }
      return;
    }

  g_hash_table_foreach (providers, (GHFunc) _insert_providers, self);

  _subscribe_to_activatable_services_changed (self);

  while (sPendingTasks)
    {
      GTask *task = sPendingTasks->data;
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      sPendingTasks = g_slist_delete_link (sPendingTasks, sPendingTasks);
    }
}

static void
_on_bus_get (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  GCancellable *cancellable = g_task_get_task_data (task);
  SpielRegistry *self = g_task_get_source_object (task);
  GError *error = NULL;

  self->connection = g_bus_get_finish (result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  spiel_collect_providers (self->connection, cancellable,
                           _on_providers_collected, task);
}

void
spiel_registry_get (GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (sRegistry != NULL)
    {
      GTask *task = g_task_new (g_object_ref (sRegistry), cancellable, callback,
                                user_data);
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }
  else if (sPendingTasks)
    {
      GObject *source_object = g_task_get_source_object (sPendingTasks->data);
      GTask *task = g_task_new (g_object_ref (source_object), cancellable,
                                callback, user_data);
      sPendingTasks = g_slist_append (sPendingTasks, task);
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
  g_autoptr (GObject) source_object = g_async_result_get_source_object (result);
  g_assert (source_object != NULL);

  g_return_val_if_fail (G_IS_ASYNC_INITABLE (source_object), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        result, error);
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
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable),
                        NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

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
      g_debug ("libspiel settings schema is not installed");
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

  g_assert (!sPendingTasks);
  sPendingTasks = g_slist_append (sPendingTasks, task);

  self->providers = g_list_store_new (SPIEL_TYPE_PROVIDER);
  self->voices = spiel_voices_list_model_new (G_LIST_MODEL (self->providers));
  self->settings = _settings_new ();

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

  g_clear_object (&self->providers);
  g_clear_object (&self->voices);
  g_clear_object (&self->settings);
  if (self->connection)
    {
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->subscription_ids[0]);
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->subscription_ids[1]);
      g_clear_object (&self->connection);
    }

  G_OBJECT_CLASS (spiel_registry_parent_class)->finalize (object);
  sRegistry = NULL;
}

static gboolean
initable_init (GInitable *initable, GCancellable *cancellable, GError **error)
{
  SpielRegistry *self = SPIEL_REGISTRY (initable);
  g_autoptr (GHashTable) providers = NULL;
  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);

  if (error && *error != NULL)
    {
      g_warning ("Error retrieving session bus: %s\n", (*error)->message);
      return FALSE;
    }

  providers = spiel_collect_providers_sync (bus, cancellable, error);

  if (error && *error != NULL)
    {
      g_warning ("Error retrieving providers: %s\n", (*error)->message);
      return FALSE;
    }

  if (providers)
    {
      g_hash_table_foreach (providers, (GHFunc) _insert_providers, self);
    }

  self->connection = g_object_ref (bus);

  _subscribe_to_activatable_services_changed (self);

  return TRUE;
}

static void
spiel_registry_init (SpielRegistry *self)
{
  self->providers = g_list_store_new (SPIEL_TYPE_PROVIDER);
  self->voices = spiel_voices_list_model_new (G_LIST_MODEL (self->providers));
  self->settings = _settings_new ();
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
  g_autoptr (SpielProvider) voice_provider = NULL;

  g_return_val_if_fail (SPIEL_IS_REGISTRY (self), NULL);
  g_return_val_if_fail (SPIEL_IS_VOICE (voice), NULL);

  voice_provider = spiel_voice_get_provider (voice);
  g_return_val_if_fail (SPIEL_IS_PROVIDER (voice_provider), NULL);

  return spiel_provider_get_proxy (voice_provider);
}

static SpielVoice *
_get_voice_from_provider_and_name (SpielRegistry *self,
                                   const char *provider_name,
                                   const char *voice_id)
{
  SpielProvider *provider =
      _get_provider_by_name (self->providers, provider_name, NULL);
  g_return_val_if_fail (provider, NULL);

  return spiel_provider_get_voice_by_id (provider, voice_id);
}

static SpielVoice *
_get_fallback_voice (SpielRegistry *self, const char *language)
{
  if (language)
    {
      guint voices_count =
          g_list_model_get_n_items (G_LIST_MODEL (self->voices));

      for (guint i = 0; i < voices_count; i++)
        {
          SpielVoice *voice = SPIEL_VOICE (
              g_list_model_get_object (G_LIST_MODEL (self->voices), i));
          if (g_strv_contains (spiel_voice_get_languages (voice), language))
            {
              return voice;
            }
          g_object_unref (voice); // Just want to borrow a ref.
        }
    }

  return g_list_model_get_item ((GListModel *) self->voices, 0);
}

SpielVoice *
spiel_registry_get_voice_for_utterance (SpielRegistry *self,
                                        SpielUtterance *utterance)
{
  g_autofree char *provider_name = NULL;
  g_autofree char *voice_id = NULL;
  const char *language = NULL;
  SpielVoice *voice = NULL;

  g_return_val_if_fail (SPIEL_IS_REGISTRY (self), NULL);
  g_return_val_if_fail (SPIEL_IS_UTTERANCE (utterance), NULL);

  voice = spiel_utterance_get_voice (utterance);
  if (voice)
    {
      return voice;
    }

  language = spiel_utterance_get_language (utterance);
  if (language && self->settings)
    {
      g_autoptr (GVariant) mapping =
          g_settings_get_value (self->settings, "language-voice-mapping");
      g_autofree char *_lang = g_strdup (language);
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
    }

  if (!provider_name && self->settings)
    {
      g_settings_get (self->settings, "default-voice", "m(ss)", NULL,
                      &provider_name, &voice_id);
    }

  if (provider_name)
    {
      g_assert (voice_id != NULL);
      voice = _get_voice_from_provider_and_name (self, provider_name, voice_id);
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
  g_return_val_if_fail (SPIEL_IS_REGISTRY (self), NULL);

  return G_LIST_MODEL (self->voices);
}

GListModel *
spiel_registry_get_providers (SpielRegistry *self)
{
  g_return_val_if_fail (SPIEL_IS_REGISTRY (self), NULL);

  return G_LIST_MODEL (self->providers);
}
