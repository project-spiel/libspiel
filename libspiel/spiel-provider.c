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

#include "spiel-provider-private.h"
#include "spiel-provider-proxy.h"
#include "spiel-provider.h"
#include "spiel-portal-helpers.h"
#include <glib-unix.h>

typedef struct
{
  GDBusConnection *connection;
  char *session_handle;
  char *well_known_name;
  char *name;
  int voices_changed_handler_id;
} _PortalInfo;

static void
_free_portal_info (_PortalInfo *portal_info)
{
  if (portal_info != NULL)
    {
      g_clear_object (&portal_info->connection);
      g_clear_pointer (&portal_info->session_handle, g_free);
      g_clear_pointer (&portal_info->well_known_name, g_free);
      g_clear_pointer (&portal_info->name, g_free);
      g_free (portal_info);
    }
}

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
  _PortalInfo *portal_info;
  gboolean is_activatable;
  GListStore *voices;
  GHashTable *voices_hashset;
};

G_DEFINE_FINAL_TYPE (SpielProvider, spiel_provider, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_WELL_KNOWN_NAME,
  PROP_NAME,
  PROP_VOICES,
  PROP_IDENTIFIER,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean handle_voices_changed (SpielProviderProxy *provider_proxy,
                                       GParamSpec *spec,
                                       gpointer user_data);

static GPtrArray *_create_provider_voices (SpielProvider *self,
                                           GVariant *voices);

static void _spiel_provider_update_voices (SpielProvider *self,
                                           GPtrArray *new_voices);

static char *_object_path_from_service_name (const char *service_name);

static void _on_get_initial_voices (GObject *source,
                                    GAsyncResult *result,
                                    gpointer user_data);

static void
_on_proxy_created (GObject *source, GAsyncResult *result, gpointer user_data);

/*< private >
 * spiel_provider_new_direct: (finish-func spiel_provider_new_finish)
 * @cancellable: (nullable): optional `GCancellable`.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: User data to pass to @callback.
 *
 * Asynchronously creates a [class@Spiel.Provider] that is associated with a
 * D-Bus service.
 *
 * When the operation is finished, @callback will be invoked in the
 * thread-default main loop of the thread you are calling this method from (see
 * [method@GLib.MainContext.push_thread_default]). You can then call
 * [ctor@Spiel.Provider.new_finish] to get the result of the operation.
 */
void
spiel_provider_new_direct (GDBusConnection *connection,
                           const char *well_known_name,
                           gboolean activatable,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
  GTask *task = g_task_new (connection, cancellable, callback, user_data);
  g_autofree char *obj_path = _object_path_from_service_name (well_known_name);
  g_task_set_task_data (task, GINT_TO_POINTER (activatable), NULL);

  spiel_provider_proxy_proxy_new_for_bus (G_BUS_TYPE_SESSION, 0,
                                          well_known_name, obj_path,
                                          cancellable, _on_proxy_created, task);
}

/*< private >
 * spiel_provider_new_finish: (constructor)
 * @result: The `GAsyncResult` obtained from the `GAsyncReadyCallback` passed to
 * [func@Spiel.Provider.new_direct].
 * @error: (nullable): optional `GError`
 *
 * Finishes an operation started with [func@Spiel.Provider.new_direct].
 *
 * Returns: (transfer full): The new `SpielProvider`, or %NULL with @error set
 *
 */
SpielProvider *
spiel_provider_new_finish (GAsyncResult *result, GError **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

/*< private >
 * spiel_provider_new_with_portal: (finish-func spiel_provider_new_finish)
 * @connection: D-Bus connection
 * @session_handle: Portal session handle
 * @well_known_name: Provider well known name
 * @cancellable: (nullable): optional `GCancellable`.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: User data to pass to @callback.
 *
 * Asynchronously creates a [class@Spiel.Provider] that is listed in the speech
 * portal.
 *
 * When the operation is finished, @callback will be invoked in the
 * thread-default main loop of the thread you are calling this method from (see
 * [method@GLib.MainContext.push_thread_default]). You can then call
 * [ctor@Spiel.Provider.new_finish] to get the result of the operation.
 */
void
spiel_provider_new_with_portal (GDBusConnection *connection,
                                const char *session_handle,
                                const char *well_known_name,
                                const char *name,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
  SpielProvider *self = g_object_new (SPIEL_TYPE_PROVIDER, NULL);
  GTask *task = g_task_new (self, cancellable, callback, user_data);

  self->portal_info = g_new0 (_PortalInfo, 1);
  self->portal_info->connection = g_object_ref (connection);
  self->portal_info->session_handle = g_strdup (session_handle);
  self->portal_info->well_known_name = g_strdup (well_known_name);
  self->portal_info->name = g_strdup (name);

  _portal_get_voices (connection, session_handle, well_known_name, cancellable,
                      _on_get_initial_voices, task);
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

  if (self->provider_proxy != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->provider_proxy,
                                            handle_voices_changed, self);
      g_clear_object (&self->provider_proxy);
    }

  if (g_set_object (&self->provider_proxy, provider_proxy))
    {
      g_autofree GPtrArray *new_voices = _create_provider_voices (
          self, spiel_provider_proxy_get_voices (self->provider_proxy));

      _spiel_provider_update_voices (self, new_voices);
      g_signal_connect (self->provider_proxy, "notify::voices",
                        G_CALLBACK (handle_voices_changed), self);
    }
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

static void
_call_synthesize_done (GObject *source_object,
                       GAsyncResult *res,
                       gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  SpielProviderProxy *provider = SPIEL_PROVIDER_PROXY (source_object);
  GError *error = NULL;
  gboolean success =
      spiel_provider_proxy_call_synthesize_finish (provider, NULL, res, &error);
  if (error)
    {
      g_task_return_error (task, error);
    }
  else
    {
      g_task_return_boolean (task, success);
    }
  g_object_unref (task);
}

static void
_call_synthesize_portal_done (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GError *error = NULL;
  gboolean success = _portal_synthesize_finish (result, &error);

  if (error)
    {
      g_task_return_error (task, error);
    }
  else
    {
      g_task_return_boolean (task, success);
    }
  g_object_unref (task);
}

/*< private >
 * spiel_provider_synthesize:
 * @self: a `SpielProvider`
 * @text: (not nullable): text to synthesize
 * @voice_id: (not nullable): voice identifier
 * @pitch: synthesis pitch
 * @rate: synthesis rate
 * @is_ssml: whether the text is SSML
 * @language: (not nullable): language code
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (nullable): a `GAsyncReadyCallback` to call when the
 *     operation is complete
 * @user_data: (nullable): user data to pass to @callback
 *
 * Send synthesis request to provider
 *
 * Returns: file descriptor of the audio stream, or -1 on error
 */
int
spiel_provider_synthesize (SpielProvider *self,
                           const gchar *text,
                           const gchar *voice_id,
                           gdouble pitch,
                           gdouble rate,
                           gboolean is_ssml,
                           const gchar *language,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
  g_autoptr (GUnixFDList) fd_list = g_unix_fd_list_new ();
  GTask *task = NULL;
  int mypipe[2];
  int fd;

  g_return_val_if_fail (SPIEL_IS_PROVIDER (self), -1);
  g_return_val_if_fail (self->provider_proxy || self->portal_info, -1);

  task = g_task_new (self, cancellable, callback, user_data);
  // XXX: Emit error on failure
  g_unix_open_pipe (mypipe, 0, NULL);
  fd = g_unix_fd_list_append (fd_list, mypipe[1], NULL);

  // XXX: Emit error on failure
  close (mypipe[1]);

  if (self->portal_info)
    {
      g_assert (!self->provider_proxy);

      _portal_synthesize (self->portal_info->connection,
                          self->portal_info->session_handle,
                          self->portal_info->well_known_name, fd, text,
                          voice_id, pitch, rate, is_ssml, language, fd_list,
                          cancellable, _call_synthesize_portal_done, task);
    }
  else
    {
      spiel_provider_proxy_call_synthesize (
          self->provider_proxy, g_variant_new_handle (fd), text, voice_id,
          pitch, rate, is_ssml, language, G_DBUS_CALL_FLAGS_NONE, -1, fd_list,
          NULL, _call_synthesize_done, task);
    }

  return mypipe[0];
}

gboolean
spiel_provider_synthesize_finish (SpielProvider *self,
                                  GAsyncResult *result,
                                  GError **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
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

  if (self->portal_info)
    {
      g_assert (!self->provider_proxy);
      return self->portal_info->name;
    }

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
 * Deprecated: 1.0.4: Use spiel_provider_get_identifier() instead
 */
G_DEPRECATED
const char *
spiel_provider_get_well_known_name (SpielProvider *self)
{
  return spiel_provider_get_identifier (self);
}

/**
 * spiel_provider_get_identifier: (get-property identifier)
 * @self: a `SpielProvider`
 *
 * Gets the provider's unique identifier.
 *
 * Returns: (transfer none): the identifier.
 *
 * Since: 1.0.4
 */
const char *
spiel_provider_get_identifier (SpielProvider *self)
{
  g_return_val_if_fail (SPIEL_IS_PROVIDER (self), NULL);

  if (self->portal_info)
    {
      g_assert (!self->provider_proxy);
      return self->portal_info->well_known_name;
    }

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

static char *
_object_path_from_service_name (const char *service_name)
{
  char **split_name = g_strsplit (service_name, ".", 0);
  g_autofree char *partial_path = g_strjoinv ("/", split_name);
  char *obj_path = g_strdup_printf ("/%s", partial_path);
  g_strfreev (split_name);
  return obj_path;
}

static GPtrArray *
_create_provider_voices (SpielProvider *self, GVariant *voices)
{
  gsize voices_count = voices ? g_variant_n_children (voices) : 0;
  GPtrArray *voices_array = g_ptr_array_new_full (voices_count, g_object_unref);

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
                     identifier, spiel_provider_get_identifier (self));
        }
      voice = g_object_new (SPIEL_TYPE_VOICE, "name", name, "identifier",
                            identifier, "languages", languages, "provider",
                            self, "features", features, NULL);
      spiel_voice_set_output_format (voice, output_format);

      g_ptr_array_add (voices_array, voice);
    }

  return voices_array;
}

static void
_spiel_provider_update_voices (SpielProvider *self, GPtrArray *new_voices)
{
  g_autoptr (GHashTable) new_voices_hashset = NULL;

  if (g_hash_table_size (self->voices_hashset) > 0)
    {
      // We are adding voices to an already populated provider, store
      // new voices in a hashset for easy purge of ones that were removed.
      new_voices_hashset = g_hash_table_new ((GHashFunc) spiel_voice_hash,
                                             (GCompareFunc) spiel_voice_equal);
    }

  if (new_voices)
    {
      for (guint i = 0; i < new_voices->len; i++)
        {
          SpielVoice *voice = new_voices->pdata[i];
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
}

static gboolean
handle_voices_changed (SpielProviderProxy *provider_proxy,
                       GParamSpec *spec,
                       gpointer user_data)
{
  SpielProvider *self = user_data;
  g_autofree char *name_owner =
      g_dbus_proxy_get_name_owner (G_DBUS_PROXY (self->provider_proxy));
  g_autofree GPtrArray *new_voices = NULL;

  if (name_owner == NULL && self->is_activatable)
    {
      // Got a change notification because an activatable service left the bus.
      // Its voices are still valid, though.
      return TRUE;
    }

  new_voices = _create_provider_voices (
      self, spiel_provider_proxy_get_voices (self->provider_proxy));

  _spiel_provider_update_voices (self, new_voices);

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

  return g_strcmp0 (spiel_provider_get_identifier (self),
                    spiel_provider_get_identifier (other));
}

static void
spiel_provider_finalize (GObject *object)
{
  SpielProvider *self = (SpielProvider *) object;

  if (self->provider_proxy != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->provider_proxy,
                                            handle_voices_changed, self);
      g_clear_object (&self->provider_proxy);
    }

  g_clear_object (&(self->voices));

  if (self->portal_info != NULL)
    {
      g_clear_pointer (&self->portal_info, _free_portal_info);
    }

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
      g_value_set_string (value, spiel_provider_get_identifier (self));
      break;
    case PROP_VOICES:
      g_value_set_object (value, spiel_provider_get_voices (self));
      break;
    case PROP_IDENTIFIER:
      g_value_set_string (value, spiel_provider_get_identifier (self));
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
   * Deprecated: 1.0.4: Use #SpielProvider:identifier instead.
   */
  properties[PROP_WELL_KNOWN_NAME] = g_param_spec_string (
      "well-known-name", NULL, NULL, NULL /* default value */,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_DEPRECATED);

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

  /**
   * SpielProvider:identifier: (getter get_identifier)
   *
   * The provider's unique identifer.
   *
   * Since: 1.0.4
   */
  properties[PROP_IDENTIFIER] =
      g_param_spec_string ("identifier", NULL, NULL, NULL /* default value */,
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

static void
_on_proxy_created (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  SpielProvider *self = g_object_new (SPIEL_TYPE_PROVIDER, NULL);
  gboolean activatable = GPOINTER_TO_INT (g_task_get_task_data (task));
  g_autoptr (GPtrArray) new_voices = NULL;
  GError *error = NULL;

  self->provider_proxy =
      spiel_provider_proxy_proxy_new_for_bus_finish (result, &error);
  self->is_activatable = activatable;

  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);

      return;
    }

  g_signal_connect (self->provider_proxy, "notify::voices",
                    G_CALLBACK (handle_voices_changed), self);

  new_voices = _create_provider_voices (
      self, spiel_provider_proxy_get_voices (self->provider_proxy));

  _spiel_provider_update_voices (self, new_voices);

  g_task_return_pointer (task, self, NULL);
  g_object_unref (task);
}

static void
_handle_portal_voices_changed (GVariant *voices_variant, gpointer user_data)
{
  SpielProvider *self = SPIEL_PROVIDER (user_data);
  g_autoptr (GPtrArray) voices = _create_provider_voices (self, voices_variant);
  _spiel_provider_update_voices (self, voices);
}

static void
_on_get_initial_voices (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
  GTask *task = user_data;
  SpielProvider *self = SPIEL_PROVIDER (g_task_get_source_object (task));
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) voices_variant =
      _portal_get_voices_finish (result, &error);
  g_autoptr (GPtrArray) voices = _create_provider_voices (self, voices_variant);
  _spiel_provider_update_voices (self, voices);
  self->portal_info->voices_changed_handler_id =
      _portal_subscribe_to_voices_changed (self->portal_info->connection,
                                           self->portal_info->session_handle,
                                           self->portal_info->well_known_name,
                                           _handle_portal_voices_changed, self);

  g_task_return_pointer (task, self, g_object_unref);
  g_object_unref (task);
}
