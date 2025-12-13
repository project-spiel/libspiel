/* spiel-portal-helpers.c
 *
 * Copyright (C) 2025 Eitan Isaacson <eitan@monotonous.org>
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

#include "spiel-portal-helpers.h"

#include "spiel-voice.h"

#include <glib-unix.h>

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define REQUEST_PATH_PREFIX "/org/freedesktop/portal/desktop/request/"
#define SESSION_PATH_PREFIX "/org/freedesktop/portal/desktop/session/"
#define REQUEST_INTERFACE "org.freedesktop.portal.Request"
#define SESSION_INTERFACE "org.freedesktop.portal.Session"
#define SETTINGS_INTERFACE "org.freedesktop.portal.Settings"

static char *
_generate_token (void)
{
  return g_strdup_printf ("spiel_%d", g_random_int_range (0, G_MAXINT));
}

static char *
_get_request_path (GDBusConnection *connection, const char *token)
{
  g_autofree GString *sender =
      g_string_new (g_dbus_connection_get_unique_name (connection) + 1);
  g_string_replace (sender, ".", "_", 0);

  return g_strconcat (REQUEST_PATH_PREFIX, sender->str, "/", token, NULL);
}

static void
_setup_response_handler (GTask *task,
                         GDBusConnection *connection,
                         const char *token,
                         const char *method_name,
                         GDBusSignalCallback callback)
{
  g_autofree char *request_path = _get_request_path (connection, token);
  guint signal_id = g_dbus_connection_signal_subscribe (
      connection, PORTAL_BUS_NAME, REQUEST_INTERFACE, "Response", request_path,
      NULL, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, callback, task, NULL);
  g_task_set_task_data (task, GUINT_TO_POINTER (signal_id), NULL);
  g_object_set_data_full (G_OBJECT (task), "method-name",
                          g_strdup (method_name), g_free);
}

static void
_on_response_getter_call (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
  GTask *task = user_data;
  GDBusConnection *connection = G_DBUS_CONNECTION (source);
  GError *error = NULL;
  g_autoptr (GVariant) ret =
      g_dbus_connection_call_finish (connection, result, &error);

  if (error)
    {
      const char *method_name =
          (const char *) g_object_get_data (G_OBJECT (task), "method-name");
      g_warning ("Failed to get response handler for '%s': %s\n", method_name,
                 error->message);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }
}

static void
_on_portal_session_created (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
  GTask *task = user_data;
  GDBusConnection *connection = G_DBUS_CONNECTION (source);
  GError *error = NULL;
  g_autoptr (GVariant) ret =
      g_dbus_connection_call_finish (connection, result, &error);
  char *session_handle = NULL;

  if (error)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_variant_get (ret, "(o)", &session_handle);

  g_task_return_pointer (task, session_handle, g_free);
  g_object_unref (task);
}

void
_portal_create_session (GDBusConnection *connection,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
  GTask *task = g_task_new (connection, cancellable, callback, user_data);
  g_autofree char *token = _generate_token ();
  GVariantBuilder options;
  g_autoptr (GVariantType) return_type = g_variant_type_new ("(o)");

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "session_handle_token",
                         g_variant_new_string (token));
  g_dbus_connection_call (connection, PORTAL_BUS_NAME, PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Speech", "CreateSession",
                          g_variant_new ("(a{sv})", &options), return_type,
                          G_DBUS_CALL_FLAGS_NONE, -1, cancellable,
                          _on_portal_session_created, task);
}

char *
_portal_create_session_finish (GAsyncResult *result, GError **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
_handle_get_providers_response (GDBusConnection *connection,
                                const gchar *sender_name,
                                const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *signal_name,
                                GVariant *parameters,
                                gpointer user_data)
{
  GTask *task = (GTask *) user_data;
  guint signal_id = GPOINTER_TO_UINT (g_task_get_task_data (task));
  g_autoptr (GVariant) results = g_variant_get_child_value (parameters, 1);
  GVariantIter iter;
  GVariant *result;
  char *result_key;
  GHashTable *providers = NULL;

  g_dbus_connection_signal_unsubscribe (connection, signal_id);

  g_variant_iter_init (&iter, results);
  while (g_variant_iter_loop (&iter, "{sv}", &result_key, &result))
    {
      if (g_str_equal (result_key, "providers"))
        {
          GVariantIter providers_iter;
          char *well_known_name = NULL;
          char *name = NULL;

          providers =
              g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
          g_variant_iter_init (&providers_iter, result);
          while (g_variant_iter_next (&providers_iter, "(ss)", &well_known_name,
                                      &name))
            {
              gboolean inserted =
                  g_hash_table_insert (providers, well_known_name, name);
              g_assert (inserted);
            }
        }
      else
        {
          g_warning ("Only expected result is 'providers', but found '%s'",
                     result_key);
        }
    }

  if (providers)
    {
      g_task_return_pointer (task, providers,
                             (GDestroyNotify) g_hash_table_unref);
    }
  else
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "No providers in result");
    }
  g_object_unref (task);
}

void
_portal_get_providers (GDBusConnection *connection,
                       const char *session_handle,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
  GTask *task = g_task_new (connection, cancellable, callback, user_data);
  GVariantBuilder options;
  g_autoptr (GVariantType) return_type = g_variant_type_new ("(o)");
  g_autofree char *token = _generate_token ();
  _setup_response_handler (task, connection, token, "GetProviders",
                           _handle_get_providers_response);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token",
                         g_variant_new_string (token));
  g_dbus_connection_call (
      connection, PORTAL_BUS_NAME, PORTAL_OBJECT_PATH,
      "org.freedesktop.portal.Speech", "GetProviders",
      g_variant_new ("(osa{sv})", session_handle, "window-hndl", &options),
      return_type, G_DBUS_CALL_FLAGS_NONE, -1, cancellable,
      _on_response_getter_call, task);
}

GHashTable *
_portal_get_providers_finish (GAsyncResult *result, GError **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

typedef struct
{
  PortalProvidersChangedCallback callback;
  char *session_handle;
  gpointer user_data;
} _ProvidersChangedClosure;

static void
_providers_changed_closure_free (_ProvidersChangedClosure *closure)
{
  closure->callback = NULL;
  g_clear_pointer (&closure->session_handle, g_free);
  g_free (closure);
}

static void
_on_get_updated_providers (GObject *source,
                           GAsyncResult *result,
                           gpointer user_data)
{
  _ProvidersChangedClosure *closure = (_ProvidersChangedClosure *) user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GHashTable) providers =
      _portal_get_providers_finish (result, &error);
  if (error)
    {
      g_warning ("Can't get providers after change notification: %s\n",
                 error->message);
      return;
    }

  if (closure->callback)
    {
      (closure->callback) (providers, closure->user_data);
    }
}

static void
_handle_providers_changed (GDBusConnection *connection,
                           const gchar *sender_name,
                           const gchar *object_path,
                           const gchar *interface_name,
                           const gchar *signal_name,
                           GVariant *parameters,
                           gpointer user_data)
{
  _ProvidersChangedClosure *closure = (_ProvidersChangedClosure *) user_data;

  _portal_get_providers (connection, closure->session_handle, NULL,
                         _on_get_updated_providers, closure);
}

guint
_portal_subscribe_to_providers_changed (GDBusConnection *connection,
                                        const char *session_handle,
                                        PortalProvidersChangedCallback callback,
                                        gpointer user_data)
{
  _ProvidersChangedClosure *closure = g_new0 (_ProvidersChangedClosure, 1);
  closure->callback = callback;
  closure->session_handle = g_strdup (session_handle);
  closure->user_data = user_data;

  return g_dbus_connection_signal_subscribe (
      connection, PORTAL_BUS_NAME, "org.freedesktop.portal.Speech",
      "ProvidersChanged", PORTAL_OBJECT_PATH, session_handle,
      G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_PATH, _handle_providers_changed, closure,
      (GDestroyNotify) _providers_changed_closure_free);
}

void
_portal_unsubscribe_from_providers_changed (GDBusConnection *connection,
                                            guint subscription_id)
{
  g_dbus_connection_signal_unsubscribe (connection, subscription_id);
}

static void
_handle_get_voices_response (GDBusConnection *connection,
                             const gchar *sender_name,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *signal_name,
                             GVariant *parameters,
                             gpointer user_data)
{
  GTask *task = (GTask *) user_data;
  guint signal_id = GPOINTER_TO_UINT (g_task_get_task_data (task));
  g_autoptr (GVariant) results = g_variant_get_child_value (parameters, 1);
  g_autoptr (GHashTable) providers = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_object_unref);
  GVariantIter iter;
  GVariant *result;
  char *result_key;
  GVariant *voices = NULL;

  g_dbus_connection_signal_unsubscribe (connection, signal_id);

  g_variant_iter_init (&iter, results);
  while (g_variant_iter_loop (&iter, "{sv}", &result_key, &result))
    {
      if (g_str_equal (result_key, "voices"))
        {
          voices = g_variant_ref (result);
        }
      else
        {
          g_warning ("Only expected result is 'providers', but found '%s'",
                     result_key);
        }
    }

  if (voices)
    {
      g_task_return_pointer (task, voices, (GDestroyNotify) g_variant_unref);
    }
  else
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "No voices in result");
    }
  g_object_unref (task);
}

void
_portal_get_voices (GDBusConnection *connection,
                    const char *session_handle,
                    const char *well_known_name,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
  GTask *task = g_task_new (connection, cancellable, callback, user_data);
  GVariantBuilder options;
  g_autoptr (GVariantType) return_type = g_variant_type_new ("(o)");
  g_autofree char *token = _generate_token ();

  _setup_response_handler (task, connection, token, "GetVoices",
                           _handle_get_voices_response);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token",
                         g_variant_new_string (token));
  g_dbus_connection_call (connection, PORTAL_BUS_NAME, PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Speech", "GetVoices",
                          g_variant_new ("(ossa{sv})", session_handle,
                                         "window-hndl", well_known_name,
                                         &options),
                          return_type, G_DBUS_CALL_FLAGS_NONE, -1, cancellable,
                          _on_response_getter_call, task);
}

GVariant *
_portal_get_voices_finish (GAsyncResult *result, GError **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

typedef struct
{
  PortalVoicesChangedCallback callback;
  char *session_handle;
  char *well_known_name;
  gpointer user_data;
} _VoicesChangedClosure;

static void
_voices_changed_closure_free (_VoicesChangedClosure *closure)
{
  closure->callback = NULL;
  g_clear_pointer (&closure->session_handle, g_free);
  g_clear_pointer (&closure->well_known_name, g_free);
  g_free (closure);
}

static void
_on_get_updated_voices (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
  _VoicesChangedClosure *closure = (_VoicesChangedClosure *) user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) voices = _portal_get_voices_finish (result, &error);
  if (error)
    {
      g_warning ("Can't get voices after change notification: %s\n",
                 error->message);
      return;
    }

  if (closure->callback)
    {
      (closure->callback) (voices, closure->user_data);
    }
}

static void
_handle_voices_changed (GDBusConnection *connection,
                        const gchar *sender_name,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *signal_name,
                        GVariant *parameters,
                        gpointer user_data)
{
  _VoicesChangedClosure *closure = (_VoicesChangedClosure *) user_data;
  const char *well_known_name = NULL;

  g_variant_get_child (parameters, 1, "&s", &well_known_name);
  if (!g_str_equal (well_known_name, closure->well_known_name))
    {
      // Not for us
      return;
    }

  _portal_get_voices (connection, closure->session_handle,
                      closure->well_known_name, NULL, _on_get_updated_voices,
                      closure);
}

guint
_portal_subscribe_to_voices_changed (GDBusConnection *connection,
                                     const char *session_handle,
                                     const char *well_known_name,
                                     PortalVoicesChangedCallback callback,
                                     gpointer user_data)
{
  _VoicesChangedClosure *closure = g_new0 (_VoicesChangedClosure, 1);
  closure->callback = callback;
  closure->session_handle = g_strdup (session_handle);
  closure->well_known_name = g_strdup (well_known_name);
  closure->user_data = user_data;

  return g_dbus_connection_signal_subscribe (
      connection, PORTAL_BUS_NAME, "org.freedesktop.portal.Speech",
      "VoicesChanged", PORTAL_OBJECT_PATH, session_handle,
      G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_PATH, _handle_voices_changed, closure,
      (GDestroyNotify) _voices_changed_closure_free);
}

void
_portal_unsubscribe_from_voices_changed (GDBusConnection *connection,
                                         guint subscription_id)
{
  g_dbus_connection_signal_unsubscribe (connection, subscription_id);
}

static void
_handle_synthesize_response (GDBusConnection *connection,
                             const gchar *sender_name,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *signal_name,
                             GVariant *parameters,
                             gpointer user_data)
{
  GTask *task = (GTask *) user_data;
  guint signal_id = GPOINTER_TO_UINT (g_task_get_task_data (task));
  g_autoptr (GVariant) results = g_variant_get_child_value (parameters, 1);
  guint32 response_code;
  g_autoptr (GHashTable) providers = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_object_unref);
  GVariantIter iter;
  GVariant *result;
  char *result_key;

  g_dbus_connection_signal_unsubscribe (connection, signal_id);

  g_variant_get (parameters, "(ua{sv})", &response_code, NULL);

  if (response_code != 0)
    {
      const char *error_message = NULL;
      g_variant_iter_init (&iter, results);
      while (g_variant_iter_loop (&iter, "{sv}", &result_key, &result))
        {
          if (g_str_equal (result_key, "error-message"))
            {
              g_variant_get (result, "&s", &error_message);
            }
        }

      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "Synthesize failed: %s",
                               error_message ? error_message : "unknown error");
      g_object_unref (task);
      return;
    }
  // XXX: Examine response and raise error if needed.

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

void
_portal_synthesize (GDBusConnection *connection,
                    const char *session_handle,
                    const char *well_known_name,
                    int fd,
                    const char *text,
                    const char *voice_id,
                    gdouble pitch,
                    gdouble rate,
                    gboolean is_ssml,
                    const char *language,
                    GUnixFDList *fd_list,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
  GTask *task = g_task_new (connection, cancellable, callback, user_data);
  g_autofree char *token = _generate_token ();
  GVariantBuilder options;
  g_autoptr (GVariantType) return_type = g_variant_type_new ("(o)");

  _setup_response_handler (task, connection, token, "Synthesize",
                           _handle_synthesize_response);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token",
                         g_variant_new_string (token));

  g_dbus_connection_call_with_unix_fd_list (
      connection, PORTAL_BUS_NAME, PORTAL_OBJECT_PATH,
      "org.freedesktop.portal.Speech", "Synthesize",
      g_variant_new ("(osshssddbsa{sv})", session_handle, "window-hndl",
                     well_known_name, fd, text, voice_id, pitch, rate, is_ssml,
                     language, &options),
      return_type, G_DBUS_CALL_FLAGS_NONE, -1, fd_list, cancellable,
      _on_response_getter_call, task);
}

gboolean
_portal_synthesize_finish (GAsyncResult *result, GError **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}
