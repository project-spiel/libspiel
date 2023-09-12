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

#include "libspiel.h"

#include "spiel-registry.h"

#include <gio/gio.h>

#define PROVIDER_SUFFIX ".Speech.Provider"

struct _SpielRegistry
{
  GObject parent_instance;
};

typedef struct
{
  GDBusConnection *connection;
  guint subscription_ids[2];
  GHashTable *providers;
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
  STARTED,
  RANGE_STARTED,
  FINISHED,
  CANCELED,
  VOICES_CHANGED,
  LAST_SIGNAL
};

static guint registry_signals[LAST_SIGNAL] = { 0 };

typedef struct
{
  GCancellable *cancellable;
  GSList *provider_names;
  guint removed_count;
  guint added_count;
} _UpdateProvidersClosure;

typedef struct
{
  SpielProvider *provider;
  SpielVoice **voices;
  gsize voices_count;
} _ProviderEntry;

static void
_update_providers_closure_destroy (gpointer data)
{
  _UpdateProvidersClosure *closure = data;
  g_clear_object (&closure->cancellable);
  g_slist_free_full (closure->provider_names, g_free);

  g_slice_free (_UpdateProvidersClosure, closure);
}

static gboolean handle_speech_start (SpielProvider *provider,
                                     guint64 task_id,
                                     gpointer user_data);

static gboolean handle_speech_range (SpielProvider *provider,
                                    guint64 task_id,
                                    guint64 start,
                                    guint64 end,
                                    gpointer user_data);

static gboolean handle_speech_end (SpielProvider *provider,
                                   guint64 task_id,
                                   gpointer user_data);

static gboolean handle_voices_changed (SpielProvider *provider,
                                       gpointer user_data);

static void
_provider_entry_destroy (gpointer data)
{
  _ProviderEntry *entry = data;
  g_clear_object (&entry->provider);
  for (gsize i = 0; i < entry->voices_count; i++)
    {
      g_clear_object (&entry->voices[i]);
    }
  g_free (entry->voices);
  entry->voices_count = 0;

  g_slice_free (_ProviderEntry, entry);
}

static void _update_next_provider (GTask *task);

static gboolean
_add_provider_with_voices (SpielRegistry *self,
                           SpielProvider *provider,
                           GVariant *voices)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  char *provider_name = NULL;
  _ProviderEntry *provider_entry = g_slice_new0 (_ProviderEntry);

  provider_entry->provider = g_object_ref (provider);
  provider_entry->voices_count = g_variant_n_children (voices);
  provider_entry->voices =
      g_malloc_n (provider_entry->voices_count, sizeof (SpielVoice *));
  g_object_get (provider, "g-name", &provider_name, NULL);
  for (gsize i = 0; i < provider_entry->voices_count; i++)
    {
      const char *name = NULL;
      const char *identifier = NULL;
      const char **languages = NULL;

      g_variant_get_child (voices, i, "(&s&s^a&s)", &name, &identifier,
                           &languages);
      provider_entry->voices[i] = g_object_new (
          SPIEL_TYPE_VOICE, "name", name, "identifier", identifier, "languages",
          languages, "provider-name", provider_name, NULL);

      g_free (languages);
    }

  g_object_connect (
      provider, "object_signal::speech-start", G_CALLBACK (handle_speech_start),
      self, "object_signal::speech-range-start", G_CALLBACK (handle_speech_range), self,
      "object_signal::speech-end", G_CALLBACK (handle_speech_end), self,
      "object_signal::voices-changed", G_CALLBACK (handle_voices_changed), self,
      NULL);

  return g_hash_table_insert (priv->providers, provider_name, provider_entry);
}

static void
_on_get_voices (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  _UpdateProvidersClosure *closure = g_task_get_task_data (task);
  SpielRegistry *self = g_task_get_source_object (task);
  GSList *next_provider = closure->provider_names;
  SpielProvider *provider = SPIEL_PROVIDER (source);
  GVariant *voices = NULL;
  GError *error = NULL;

  g_assert (next_provider != NULL);
  spiel_provider_call_get_voices_finish (provider, &voices, result, &error);

  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  if (_add_provider_with_voices (self, g_object_ref (provider), voices))
    {
      closure->added_count++;
    }

  closure->provider_names =
      g_slist_remove_link (closure->provider_names, next_provider);
  g_slist_free_full (next_provider, g_free);
  _update_next_provider (task);
}

static void
_on_provider_created (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  _UpdateProvidersClosure *closure = g_task_get_task_data (task);
  SpielProvider *provider = NULL;
  GSList *next_provider = closure->provider_names;
  GError *error = NULL;
  g_assert (next_provider != NULL);

  provider = spiel_provider_proxy_new_for_bus_finish (result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  spiel_provider_call_get_voices (provider, closure->cancellable,
                                  _on_get_voices, task);

  g_object_unref (provider);
}

static void
_update_next_provider (GTask *task)
{
  _UpdateProvidersClosure *closure = g_task_get_task_data (task);
  SpielRegistry *self = g_task_get_source_object (task);
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  GSList *next_provider = closure->provider_names;
  char *service_name = NULL;
  char **split_name = NULL;
  char *partial_path = NULL;
  char *obj_path = NULL;

  if (next_provider == NULL)
    {
      g_task_return_boolean (task, closure->removed_count > 0 ||
                                       closure->added_count > 0);
      g_object_unref (task);
      return;
    }

  service_name = (char *) next_provider->data;
  if (g_hash_table_contains (priv->providers, service_name))
    {
      closure->provider_names =
          g_slist_remove_link (closure->provider_names, next_provider);
      g_slist_free_full (next_provider, g_free);
      _update_next_provider (task);
      return;
    }

  split_name = g_strsplit (service_name, ".", 0);
  partial_path = g_strjoinv ("/", split_name);
  obj_path = g_strdup_printf ("/%s", partial_path);
  g_strfreev (split_name);
  g_free (partial_path);

  spiel_provider_proxy_new_for_bus (
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
      service_name, obj_path, closure->cancellable, _on_provider_created, task);

  g_free (obj_path);
}

static gboolean
update_providers_finished (GObject *source, GAsyncResult *res, GError **error);

static void update_providers (SpielRegistry *self,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data);

static void
_on_providers_updated (GObject *source, GAsyncResult *res, gpointer user_data)
{
  SpielRegistry *self = SPIEL_REGISTRY (source);
  GError *err = NULL;
  gboolean changed = update_providers_finished (source, res, &err);
  if (err != NULL)
    {
      g_warning ("Error updating providers: %s\n", err->message);
      g_error_free (err);
    }

  if (changed)
    {
      g_signal_emit (self, registry_signals[VOICES_CHANGED], 0);
    }
}

static void
_providers_maybe_changed (GDBusConnection *connection,
                          const gchar *sender_name,
                          const gchar *object_path,
                          const gchar *interface_name,
                          const gchar *signal_name,
                          GVariant *parameters,
                          gpointer user_data)
{
  SpielRegistry *self = user_data;
  update_providers (self, NULL, _on_providers_updated, NULL);
}

static void
_subscribe_to_activatable_services_changed (SpielRegistry *self,
                                            GDBusConnection *connection)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  priv->connection = g_object_ref (connection);
  priv->subscription_ids[0] = g_dbus_connection_signal_subscribe (
      connection, "org.freedesktop.DBus", "org.freedesktop.DBus",
      "ActivatableServicesChanged", "/org/freedesktop/DBus", NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, _providers_maybe_changed, self, NULL);

  // priv->subscription_ids[1] = g_dbus_connection_signal_subscribe (
  //     connection, "org.freedesktop.DBus", "org.freedesktop.DBus",
  //     "NameOwnerChanged", "/org/freedesktop/DBus", NULL,
  //     G_DBUS_SIGNAL_FLAGS_NONE, _providers_maybe_changed,
  //     g_object_ref (self), g_object_unref);
}

static void
_collect_provider_names (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
  GTask *task = user_data;
  _UpdateProvidersClosure *closure = g_task_get_task_data (task);
  GError *error = NULL;
  GDBusConnection *bus = G_DBUS_CONNECTION (source);
  GVariant *real_ret = NULL;
  GVariantIter iter;
  GVariant *service = NULL;
  GVariant *ret = g_dbus_connection_call_finish (bus, result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  real_ret = g_variant_get_child_value (ret, 0);
  g_variant_unref (ret);

  g_variant_iter_init (&iter, real_ret);
  while ((service = g_variant_iter_next_value (&iter)))
    {
      const char *service_name = g_variant_get_string (service, NULL);
      if (!g_str_has_suffix (service_name, PROVIDER_SUFFIX))
        {
          continue;
        }
      closure->provider_names =
          g_slist_prepend (closure->provider_names, g_strdup (service_name));
      g_variant_unref (service);
    }
  g_variant_unref (real_ret);
}

static gboolean
_purge_providers (gpointer key, gpointer value, gpointer user_data)
{
  const char *provider_name = key;
  GSList *provider_names = user_data;

  return g_slist_find_custom (provider_names, provider_name,
                              (GCompareFunc) g_strcmp0) == NULL;
}

static void
_update_providers_on_list_names (GObject *source,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
  GTask *task = user_data;
  _UpdateProvidersClosure *closure = g_task_get_task_data (task);
  SpielRegistry *self = g_task_get_source_object (task);
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);

  _collect_provider_names (source, result, user_data);
  closure->removed_count = g_hash_table_foreach_remove (
      priv->providers, _purge_providers, closure->provider_names);

  _update_next_provider (task);
}

static void
_update_providers_on_list_activatable_names (GObject *source,
                                             GAsyncResult *result,
                                             gpointer user_data)
{
  GTask *task = user_data;
  _UpdateProvidersClosure *closure = g_task_get_task_data (task);
  SpielRegistry *self = g_task_get_source_object (task);
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);

  _collect_provider_names (source, result, user_data);

  g_dbus_connection_call (
      priv->connection, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "ListNames", NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
      -1, closure->cancellable, _update_providers_on_list_names, task);
}

static void
update_providers (SpielRegistry *self,
                  GCancellable *cancellable,
                  GAsyncReadyCallback callback,
                  gpointer user_data)
{
  GTask *task = g_task_new (self, cancellable, callback, user_data);
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  _UpdateProvidersClosure *closure = g_slice_new0 (_UpdateProvidersClosure);

  closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  g_task_set_task_data (task, closure,
                        (GDestroyNotify) _update_providers_closure_destroy);
  g_dbus_connection_call (priv->connection, "org.freedesktop.DBus",
                          "/org/freedesktop/DBus", "org.freedesktop.DBus",
                          "ListActivatableNames", NULL, NULL,
                          G_DBUS_CALL_FLAGS_NONE, -1, closure->cancellable,
                          _update_providers_on_list_activatable_names, task);
}

static gboolean
update_providers_finished (GObject *source, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (g_task_is_valid (res, source), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

static void
_on_providers_created (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GTask *task = user_data;
  GError *err = NULL;
  update_providers_finished (source, res, &err);
  if (err != NULL)
    {
      g_warning ("Error creating providers: %s\n", err->message);
      g_error_free (err);
    }

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static void
_on_bus_get (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  GCancellable *cancellable = g_task_get_task_data (task);
  SpielRegistry *self = g_task_get_source_object (task);

  GError *error = NULL;
  GDBusConnection *bus = g_bus_get_finish (result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  _subscribe_to_activatable_services_changed (self, bus);

  update_providers (self, cancellable, _on_providers_created, task);
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
      sRegistry =
          g_initable_new (SPIEL_TYPE_REGISTRY, cancellable, error, NULL);
    }
  else
    {
      g_object_ref (sRegistry);
    }

  return sRegistry;
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
  if (priv->connection)
    {
      g_dbus_connection_signal_unsubscribe (priv->connection,
                                            priv->subscription_ids[0]);
      // g_dbus_connection_signal_unsubscribe (priv->connection,
      //                                       priv->subscription_ids[1]);
      g_object_unref (priv->connection);
    }

  G_OBJECT_CLASS (spiel_registry_parent_class)->finalize (object);
  g_assert (object == G_OBJECT (sRegistry));
  sRegistry = NULL;
}

static void
_add_provider (SpielRegistry *self, const char *service_name)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  SpielProvider *provider = NULL;
  char **split_name = NULL;
  char *partial_path = NULL;
  char *obj_path = NULL;
  GError *err = NULL;
  GVariant *voices = NULL;

  if (!g_str_has_suffix (service_name, PROVIDER_SUFFIX))
    {
      return;
    }

  if (g_hash_table_contains (priv->providers, service_name))
    {
      return;
    }

  split_name = g_strsplit (service_name, ".", 0);
  partial_path = g_strjoinv ("/", split_name);
  obj_path = g_strdup_printf ("/%s", partial_path);
  g_strfreev (split_name);
  g_free (partial_path);
  provider = spiel_provider_proxy_new_for_bus_sync (
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
      service_name, obj_path, NULL, &err);
  g_free (obj_path);
  if (err)
    {
      g_warning ("Error initializing provider: %s\n", err->message);
      g_error_free (err);
    }

  spiel_provider_call_get_voices_sync (provider, &voices, NULL, &err);
  if (err)
    {
      g_warning ("Error getting voices: %s\n", err->message);
      g_error_free (err);
    }

  _add_provider_with_voices (self, provider, voices);
}

static gboolean
initable_init (GInitable *initable, GCancellable *cancellable, GError **error)
{
  SpielRegistry *self = SPIEL_REGISTRY (initable);
  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  const char *list_name_methods[] = { "ListActivatableNames", "ListNames",
                                      NULL };
  _subscribe_to_activatable_services_changed (self, bus);
  for (const char **method = list_name_methods; *method; method++)
    {
      GVariant *real_ret = NULL;
      GVariantIter iter;
      GVariant *service = NULL;
      GError *err = NULL;
      GVariant *ret = g_dbus_connection_call_sync (
          bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
          "org.freedesktop.DBus", *method, NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
          -1, NULL, &err);
      if (err)
        {
          g_warning ("Error calling list (%s): %s\n", *method, err->message);
          g_error_free (err);
          continue;
        }

      real_ret = g_variant_get_child_value (ret, 0);
      g_variant_unref (ret);

      g_variant_iter_init (&iter, real_ret);
      while ((service = g_variant_iter_next_value (&iter)))
        {
          _add_provider (self, g_variant_get_string (service, NULL));
          g_variant_unref (service);
        }
      g_variant_unref (real_ret);
    }

  return TRUE;
}

static void
spiel_registry_init (SpielRegistry *self)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  priv->providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                           _provider_entry_destroy);
}

static void
spiel_registry_class_init (SpielRegistryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_registry_finalize;

  registry_signals[STARTED] =
      g_signal_new ("started", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
                    NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT64);

  registry_signals[RANGE_STARTED] = g_signal_new (
      "range-started", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 3, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_UINT64);

  registry_signals[FINISHED] =
      g_signal_new ("finished", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
                    0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT64);

  registry_signals[CANCELED] =
      g_signal_new ("canceled", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
                    0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT64);

  registry_signals[VOICES_CHANGED] =
      g_signal_new ("voices-changed", G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
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
handle_speech_start (SpielProvider *provider,
                     guint64 task_id,
                     gpointer user_data)
{
  SpielRegistry *self = user_data;
  g_signal_emit (self, registry_signals[STARTED], 0, task_id);
  return TRUE;
}

static gboolean
handle_speech_range (SpielProvider *provider,
                    guint64 task_id,
                    guint64 start,
                    guint64 end,
                    gpointer user_data)
{
  SpielRegistry *self = user_data;
  g_signal_emit (self, registry_signals[RANGE_STARTED], 0, task_id, start, end);
  return TRUE;
}

static gboolean
handle_speech_end (SpielProvider *provider, guint64 task_id, gpointer user_data)
{
  SpielRegistry *self = user_data;
  g_signal_emit (self, registry_signals[FINISHED], 0, task_id);
  return TRUE;
}

static void
_on_get_voices_maybe_changed (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data)
{
  SpielRegistry *self = user_data;
  SpielProvider *provider = SPIEL_PROVIDER (source);
  GVariant *voices = NULL;
  GError *error = NULL;

  spiel_provider_call_get_voices_finish (provider, &voices, result, &error);

  if (error != NULL)
    {
      g_warning ("Failed to get updated voice list: %s", error->message);
      g_free (error);
      return;
    }

  _add_provider_with_voices (self, provider, voices);
  g_signal_emit (self, registry_signals[VOICES_CHANGED], 0);
}

static gboolean
handle_voices_changed (SpielProvider *provider, gpointer user_data)
{
  SpielRegistry *self = user_data;

  spiel_provider_call_get_voices (provider, NULL, _on_get_voices_maybe_changed,
                                  self);

  return TRUE;
}

/* Public API */

SpielProvider *
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

SpielVoice *
spiel_registry_get_voice_for_utterance (SpielRegistry *self,
                                        SpielUtterance *utterance)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  SpielVoice *voice = NULL;
  g_object_get (utterance, "voice", &voice, NULL);

  if (voice == NULL)
    {
      GList *providers = g_hash_table_get_values (priv->providers);
      _ProviderEntry *provider_entry = providers->data;
      if (provider_entry && provider_entry->voices_count > 0)
        voice = provider_entry->voices[0];
      g_list_free (providers);
    }

  return voice;
}

static void
_collate_voices (gpointer key, gpointer value, gpointer user_data)
{
  _ProviderEntry *provider_entry = value;
  GListStore *voices = user_data;
  g_list_store_splice (voices, 0, 0, (gpointer *) provider_entry->voices,
                       provider_entry->voices_count);
}

GListStore *
spiel_registry_get_voices (SpielRegistry *self)
{
  SpielRegistryPrivate *priv = spiel_registry_get_instance_private (self);
  GListStore *voices = g_list_store_new (SPIEL_TYPE_VOICE);
  g_hash_table_foreach (priv->providers, _collate_voices, voices);
  return voices;
}