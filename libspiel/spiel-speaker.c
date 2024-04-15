/* spiel-speaker.c
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

#include "spiel-speaker.h"

#include "spiel-provider-proxy.h"
#include "spiel-provider-src.h"
#include "spiel-registry.h"
#include "spiel-voice.h"
#include <fcntl.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <gst/audio/audio.h>
#include <gst/gst.h>

/**
 * SpielSpeaker:
 *
 * A virtual speaker for speech synthesis
 *
 * The `SpielSpeaker` class represents a single "individual" speaker. Its
 * primary method is [method@Spiel.Speaker.speak] which queues utterances to be
 * spoken.
 *
 * This class also provides a list of available voices provided by DBus speech
 * providers that are activatable on the session bus.
 *
 * `SpielSpeaker`'s initialization may perform blocking IO if it the first
 * instance in the process. The default constructor is asynchronous
 * ([func@Spiel.Speaker.new]), although there is a synchronous blocking
 * alternative ([ctor@Spiel.Speaker.new_sync]).
 *
 * Since: 1.0
 */

struct _SpielSpeaker
{
  GObject parent_instance;
  gboolean paused;
  SpielRegistry *registry;
  GSList *queue;
  GstElement *pipeline;
  GstElement *convert;
  GstElement *sink;
};

static void initable_iface_init (GInitableIface *initable_iface);
static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SpielSpeaker,
    spiel_speaker,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                               async_initable_iface_init))

enum
{
  UTTURANCE_STARTED,
  UTTERANCE_FINISHED,
  UTTERANCE_CANCELED,
  UTTERANCE_ERROR,
  RANGE_STARTED,
  WORD_STARTED,
  SENTENCE_STARTED,
  MARK_REACHED,
  LAST_SIGNAL
};

static guint speaker_signals[LAST_SIGNAL] = { 0 };

enum
{
  PROP_0,
  PROP_SPEAKING,
  PROP_PAUSED,
  PROP_VOICES,
  PROP_PROVIDERS,
  PROP_SINK,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

typedef struct
{
  SpielUtterance *utterance;
  GstElement *src;
  GstElement *parse;
  GstElement *volume;
  GError *error;
  gboolean started;
  GSList *deferred_messages;
} _QueueEntry;

static void
_queue_entry_destroy (gpointer data)
{
  _QueueEntry *entry = data;
  if (entry)
    {
      g_clear_object (&entry->utterance);
      if (entry->error)
        {
          g_error_free (entry->error);
        }

      if (entry->src)
        {
          gint fd = -1;
          g_object_get (entry->src, "fd", &fd, NULL);
          if (fd > 0)
            {
              close (fd);
            }
          g_clear_object (&entry->src);
        }

      if (entry->parse)
        {
          g_clear_object (&entry->parse);
        }

      if (entry->volume)
        {
          g_clear_object (&entry->volume);
        }

      if (entry->deferred_messages)
        {
          g_slist_free_full (g_steal_pointer (&entry->deferred_messages),
                             (GDestroyNotify) gst_message_unref);
        }
    }

  g_slice_free (_QueueEntry, entry);
}

/**
 * spiel_speaker_new:
 * @cancellable: (nullable): optional `GCancellable`.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: User data to pass to @callback.
 *
 * Asynchronously creates a [class@Spiel.Speaker].
 *
 * When the operation is finished, @callback will be invoked in the
 * thread-default main loop of the thread you are calling this method from (see
 * [method@GLib.MainContext.push_thread_default]). You can then call
 * [ctor@Spiel.Speaker.new_finish] to get the result of the operation.
 *
 * See [ctor@Spiel.Speaker.new_sync] for the synchronous, blocking version of
 * this constructor.
 *
 * Since: 1.0
 */
void
spiel_speaker_new (GCancellable *cancellable,
                   GAsyncReadyCallback callback,
                   gpointer user_data)
{
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_async_initable_new_async (SPIEL_TYPE_SPEAKER, G_PRIORITY_DEFAULT,
                              cancellable, callback, user_data, NULL);
}

/**
 * spiel_speaker_new_finish: (constructor)
 * @result: The `GAsyncResult` obtained from the `GAsyncReadyCallback` passed to
 * [func@Spiel.Speaker.new].
 * @error: (nullable): optional `GError`
 *
 * Finishes an operation started with [func@Spiel.Speaker.new].
 *
 * Returns: (transfer full): The new `SpielSpeaker`, or %NULL with @error set
 *
 * Since: 1.0
 */
SpielSpeaker *
spiel_speaker_new_finish (GAsyncResult *result, GError **error)
{
  GObject *object;
  g_autoptr (GObject) source_object = g_async_result_get_source_object (result);

  g_return_val_if_fail (G_IS_ASYNC_INITABLE (source_object), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        result, error);
  if (object != NULL)
    return SPIEL_SPEAKER (object);
  else
    return NULL;
}

/**
 * spiel_speaker_new_sync:
 * @cancellable: (nullable): A optional `GCancellable`.
 * @error: Return location for error or %NULL
 *
 * Synchronously creates a SpielSpeaker.
 *
 * The calling thread is blocked until a reply is received.
 *
 * See spiel_speaker_new() for the asynchronous version of this constructor.
 *
 * Returns: (transfer full) (type SpielSpeaker): The constructed speaker object
 * or %NULL if @error is set.
 *
 * Since: 1.0
 */
SpielSpeaker *
spiel_speaker_new_sync (GCancellable *cancellable, GError **error)
{
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable),
                        NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_initable_new (SPIEL_TYPE_SPEAKER, cancellable, error, NULL);
}

static void
spiel_speaker_finalize (GObject *object)
{
  SpielSpeaker *self = SPIEL_SPEAKER (object);

  g_clear_object (&self->registry);
  if (self->pipeline)
    {
      gst_element_set_state (self->pipeline, GST_STATE_NULL);
    }
  g_clear_object (&self->pipeline);
  g_clear_object (&self->convert);
  g_clear_object (&self->sink);
  g_slist_free_full (g_steal_pointer (&self->queue),
                     (GDestroyNotify) _queue_entry_destroy);

  G_OBJECT_CLASS (spiel_speaker_parent_class)->finalize (object);
}

static void
spiel_speaker_get_property (GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  SpielSpeaker *self = SPIEL_SPEAKER (object);

  switch (prop_id)
    {
    case PROP_SPEAKING:
      g_value_set_boolean (value, self->queue != NULL);
      break;
    case PROP_PAUSED:
      g_value_set_boolean (value, self->paused);
      break;
    case PROP_VOICES:
      g_value_set_object (value, spiel_speaker_get_voices (self));
      break;
    case PROP_PROVIDERS:
      g_value_set_object (value, spiel_speaker_get_providers (self));
      break;
    case PROP_SINK:
      g_value_set_object (value, self->sink);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spiel_speaker_set_property (GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  SpielSpeaker *self = SPIEL_SPEAKER (object);

  switch (prop_id)
    {
    case PROP_SINK:
      {
        GstElement *new_sink = g_value_get_object (value);

        gst_element_unlink (self->convert, self->sink);
        gst_bin_remove (GST_BIN (self->pipeline), self->sink);
        g_object_unref (self->sink);

        gst_bin_add (GST_BIN (self->pipeline), new_sink);
        gst_element_link (self->convert, new_sink);
        self->sink = g_object_ref (new_sink);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spiel_speaker_class_init (SpielSpeakerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_speaker_finalize;
  object_class->get_property = spiel_speaker_get_property;
  object_class->set_property = spiel_speaker_set_property;

  /**
   * SpielSpeaker:speaking:
   *
   * The speaker has an utterance queued or speaking.
   *
   * Since: 1.0
   */
  properties[PROP_SPEAKING] =
      g_param_spec_boolean ("speaking", NULL, NULL, FALSE /* default value */,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielSpeaker:paused:
   *
   * The speaker is in a paused state.
   *
   * See [method@Spiel.Speaker.pause] and [method@Spiel.Speaker.resume].
   *
   * Since: 1.0
   */
  properties[PROP_PAUSED] =
      g_param_spec_boolean ("paused", NULL, NULL, FALSE /* default value */,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielSpeaker:voices: (getter get_voices)
   *
   * The list of available [class@Voice]s that can be used in an utterance.
   *
   * Since: 1.0
   */
  properties[PROP_VOICES] =
      g_param_spec_object ("voices", NULL, NULL, G_TYPE_LIST_MODEL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielSpeaker:providers: (getter get_providers)
   *
   * The list of available [class@Provider]s that offer voices.
   *
   * Since: 1.0
   */
  properties[PROP_PROVIDERS] =
      g_param_spec_object ("providers", NULL, NULL, G_TYPE_LIST_MODEL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielSpeaker:sink:
   *
   * The gstreamer sink this speaker is connected to.
   *
   * Since: 1.0
   */
  properties[PROP_SINK] = g_param_spec_object (
      "sink", NULL, NULL, GST_TYPE_ELEMENT, G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * SpielSpeaker::utterance-started:
   * @speaker: A `SpielSpeaker`
   * @utterance: A `SpielUtterance`
   *
   * Emitted when the given utterance is actively being spoken.
   *
   * Since: 1.0
   */
  speaker_signals[UTTURANCE_STARTED] = g_signal_new (
      "utterance-started", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
      NULL, NULL, NULL, G_TYPE_NONE, 1, SPIEL_TYPE_UTTERANCE);

  /**
   * SpielSpeaker::utterance-finished:
   * @speaker: A `SpielSpeaker`
   * @utterance: A `SpielUtterance`
   *
   * Emitted when a given utterance has completed speaking
   *
   * Since: 1.0
   */
  speaker_signals[UTTERANCE_FINISHED] = g_signal_new (
      "utterance-finished", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
      NULL, NULL, NULL, G_TYPE_NONE, 1, SPIEL_TYPE_UTTERANCE);

  /**
   * SpielSpeaker::utterance-canceled:
   * @speaker: A `SpielSpeaker`
   * @utterance: A `SpielUtterance`
   *
   * Emitted when a given utterance has been canceled after it has started
   * speaking
   *
   * Since: 1.0
   */
  speaker_signals[UTTERANCE_CANCELED] = g_signal_new (
      "utterance-canceled", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
      NULL, NULL, NULL, G_TYPE_NONE, 1, SPIEL_TYPE_UTTERANCE);

  /**
   * SpielSpeaker::utterance-error:
   * @speaker: A `SpielSpeaker`
   * @utterance: A `SpielUtterance`
   * @error: A #GError
   *
   * Emitted when a given utterance has failed to start or complete.
   *
   * Since: 1.0
   */
  speaker_signals[UTTERANCE_ERROR] = g_signal_new (
      "utterance-error", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, SPIEL_TYPE_UTTERANCE, G_TYPE_ERROR);

  /**
   * SpielSpeaker::word-started:
   * @speaker: A `SpielSpeaker`
   * @utterance: A `SpielUtterance`
   * @start: Character start offset of speech word
   * @end: Character end offset of speech word
   *
   * Emitted when a word will be spoken in a given utterance. Not all
   * voices are capable of notifying when a word will be spoken.
   *
   * Since: 1.0
   */
  speaker_signals[WORD_STARTED] =
      g_signal_new ("word-started", G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 3,
                    SPIEL_TYPE_UTTERANCE, G_TYPE_UINT64, G_TYPE_UINT64);

  /**
   * SpielSpeaker::sentence-started:
   * @speaker: A `SpielSpeaker`
   * @utterance: A `SpielUtterance`
   * @start: Character start offset of speech sentence
   * @end: Character end offset of speech sentence
   *
   * Emitted when a sentence will be spoken in a given utterance. Not all
   * voices are capable of notifying when a sentence will be spoken.
   *
   * Since: 1.0
   */
  speaker_signals[SENTENCE_STARTED] =
      g_signal_new ("sentence-started", G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 3,
                    SPIEL_TYPE_UTTERANCE, G_TYPE_UINT64, G_TYPE_UINT64);

  /**
   * SpielSpeaker::range-started:
   * @speaker: A `SpielSpeaker`
   * @utterance: A `SpielUtterance`
   * @start: Character start offset of speech range
   * @end: Character end offset of speech range
   *
   * Emitted when a range will be spoken in a given utterance. Not all
   * voices are capable of notifying when a range will be spoken.
   *
   * Since: 1.0
   */
  speaker_signals[RANGE_STARTED] =
      g_signal_new ("range-started", G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 3,
                    SPIEL_TYPE_UTTERANCE, G_TYPE_UINT64, G_TYPE_UINT64);

  /**
   * SpielSpeaker::mark-reached:
   * @speaker: A `SpielSpeaker`
   * @utterance: A `SpielUtterance`
   * @name: Name of mark reached
   *
   * Emitted when an SSML mark element is reached
   *
   * Since: 1.0
   */
  speaker_signals[MARK_REACHED] = g_signal_new (
      "mark-reached", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, SPIEL_TYPE_UTTERANCE, G_TYPE_STRING);
}

static gboolean
_handle_gst_eos (GstBus *bus, GstMessage *msg, SpielSpeaker *self);

static gboolean
_handle_gst_state_change (GstBus *bus, GstMessage *msg, SpielSpeaker *self);

static gboolean
_handle_gst_element_message (GstBus *bus, GstMessage *msg, SpielSpeaker *self);

static void _process_going_to_speak_message (GstMessage *msg,
                                             SpielSpeaker *self);

static void
_setup_pipeline (SpielSpeaker *self, GError **error)
{
  g_autoptr (GstBus) bus = NULL;
  GstElement *convert = NULL;
  GstElement *sink = NULL;

  convert = gst_element_factory_make ("audioconvert", "convert");
  if (convert == NULL)
    {
      if (error != NULL && *error == NULL)
        {
          g_set_error_literal (error, GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
                               "Failed to create 'convert' element");
        }

      return;
    }

  sink = gst_element_factory_make (
      g_getenv ("SPIEL_TEST") ? "fakesink" : "autoaudiosink", "sink");
  if (sink == NULL)
    {
      if (error != NULL && *error == NULL)
        {
          g_set_error_literal (error, GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
                               "Failed to create 'autoaudiosink' element; "
                               "ensure GStreamer Good Plug-ins are installed");
        }

      gst_object_unref (gst_object_ref_sink (convert));
      return;
    }

  self->pipeline = gst_pipeline_new ("pipeline");

  gst_bin_add_many (GST_BIN (self->pipeline), convert, sink, NULL);
  if (!gst_element_link (convert, sink))
    {
      if (error != NULL && *error == NULL)
        {
          g_set_error_literal (error, GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
                               "Failed to link 'convert' and 'sink' elements");
        }

      gst_object_unref (gst_object_ref_sink (convert));
      gst_object_unref (gst_object_ref_sink (sink));
      g_clear_pointer (&self->pipeline, gst_object_unref);
      return;
    }

  bus = gst_element_get_bus (self->pipeline);

  gst_bus_add_signal_watch (bus);
  g_object_connect (bus, "signal::message::eos", _handle_gst_eos, self,
                    "signal::message::state-changed", _handle_gst_state_change,
                    self, "signal::message::element",
                    _handle_gst_element_message, self, NULL);

  self->convert = gst_object_ref_sink (convert);
  self->sink = gst_object_ref_sink (sink);
}

static void
_on_registry_get (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  SpielSpeaker *self = g_task_get_task_data (task);
  GError *error = NULL;

  self->registry = spiel_registry_get_finish (result, &error);
  if (error == NULL)
    {
      _setup_pipeline (self, &error);
    }
  if (error != NULL)
    {
      g_task_return_error (task, error);
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }

  g_object_unref (task);
}

static void
async_initable_init_async (GAsyncInitable *initable,
                           gint io_priority,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
  GTask *task = g_task_new (initable, cancellable, callback, user_data);
  SpielSpeaker *self = SPIEL_SPEAKER (initable);

  self->paused = FALSE;
  self->queue = NULL;

  g_task_set_task_data (task, g_object_ref (self), g_object_unref);
  spiel_registry_get (cancellable, _on_registry_get, task);
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
async_initable_iface_init (GAsyncInitableIface *async_initable_iface)
{
  async_initable_iface->init_async = async_initable_init_async;
  async_initable_iface->init_finish = async_initable_init_finish;
}

static gboolean
initable_init (GInitable *initable, GCancellable *cancellable, GError **error)
{
  SpielSpeaker *self = SPIEL_SPEAKER (initable);
  GError *err = NULL;

  self->registry = spiel_registry_get_sync (cancellable, &err);
  if (err == NULL)
    {
      _setup_pipeline (self, &err);
    }

  if (err != NULL)
    {
      g_warning ("Error initializing speaker: %s\n", err->message);
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

static void
spiel_speaker_init (SpielSpeaker *self)
{
  self->paused = FALSE;
  self->queue = NULL;
}

GQuark
spiel_error_quark (void)
{
  return g_quark_from_static_string ("spiel-error-quark");
}

/*
 * Playback
 */

static void _provider_call_synthesize_done (GObject *source_object,
                                            GAsyncResult *res,
                                            gpointer user_data);

static void _speak_current_entry (SpielSpeaker *self);

static void _advance_to_next_entry_or_finish (SpielSpeaker *self,
                                              gboolean canceled);

typedef struct
{
  SpielSpeaker *self;
  // We use an utterance instead of a queue entry because we can hold a strong
  // reference.
  SpielUtterance *utterance;
} _CallSynthData;

/**
 * spiel_speaker_speak:
 * @self: a `SpielSpeaker`
 * @utterance: an `SpielUtterance` to speak
 *
 * Speak the given utterance. If an utterance is already being spoken
 * the provided utterances will be added to a queue and will be spoken
 * in the order received.
 *
 * Since: 1.0
 */
void
spiel_speaker_speak (SpielSpeaker *self, SpielUtterance *utterance)
{
  _QueueEntry *entry = g_slice_new0 (_QueueEntry);
  _CallSynthData *call_synth_data = g_slice_new0 (_CallSynthData);
  SpielProviderProxy *provider = NULL;
  GUnixFDList *fd_list = g_unix_fd_list_new ();
  int mypipe[2];
  int fd;
  const char *text = spiel_utterance_get_text (utterance);
  const char *lang = spiel_utterance_get_language (utterance);
  const char *output_format = NULL;
  const char *stream_type = NULL;
  gboolean is_ssml = FALSE;
  gdouble pitch, rate, volume;
  g_autoptr (SpielVoice) voice = NULL;
  GstStructure *gst_struct;

  g_return_if_fail (SPIEL_IS_SPEAKER (self));
  g_return_if_fail (SPIEL_IS_UTTERANCE (utterance));

  g_object_get (utterance, "pitch", &pitch, "rate", &rate, "volume", &volume,
                "voice", &voice, "is-ssml", &is_ssml, NULL);

  if (voice == NULL)
    {
      voice =
          spiel_registry_get_voice_for_utterance (self->registry, utterance);
      if (!voice)
        {
          g_warning ("No voice available!");
          return;
        }
      spiel_utterance_set_voice (utterance, voice);
      g_object_ref (voice);
    }

  provider = spiel_registry_get_provider_for_voice (self->registry, voice);

  // XXX: Emit error on failure
  g_unix_open_pipe (mypipe, 0, NULL);
  fd = g_unix_fd_list_append (fd_list, mypipe[1], NULL);

  // XXX: Emit error on failure
  close (mypipe[1]);

  call_synth_data->self = self;
  call_synth_data->utterance = g_object_ref (utterance);

  spiel_provider_proxy_call_synthesize (
      provider, g_variant_new_handle (fd), text,
      voice ? spiel_voice_get_identifier (voice) : "", pitch, rate, is_ssml,
      lang ? lang : "", G_DBUS_CALL_FLAGS_NONE, -1, fd_list, NULL,
      _provider_call_synthesize_done, call_synth_data);

  g_object_unref (fd_list);

  entry->utterance = g_object_ref (utterance);

  output_format = spiel_voice_get_output_format (voice);

  gst_struct = gst_structure_from_string (output_format, NULL);

  stream_type = gst_struct ? gst_structure_get_name (gst_struct) : NULL;

  if (g_strcmp0 (stream_type, "audio/x-raw") == 0)
    {
      entry->src =
          gst_element_factory_make_full ("fdsrc", "fd", mypipe[0], NULL);
    }
  else if (g_strcmp0 (stream_type, "audio/x-spiel") == 0)
    {
      entry->src = GST_ELEMENT (spiel_provider_src_new (mypipe[0]));
    }

  if (!entry->src)
    {
      g_assert (!entry->error);
      g_set_error (&entry->error, SPIEL_ERROR, SPIEL_ERROR_MISCONFIGURED_VOICE,
                   "Voice output format not set correctly: '%s'",
                   output_format);
    }
  else
    {
      gint sample_rate, channels;
      const char *pcm_format;

      entry->volume =
          gst_element_factory_make_full ("volume", "volume", volume, NULL);

      entry->parse = gst_element_factory_make ("rawaudioparse", "parse");
      if (gst_structure_get_int (gst_struct, "rate", &sample_rate))
        {
          g_object_set (entry->parse, "sample-rate", sample_rate, NULL);
        }

      if (gst_structure_get_int (gst_struct, "channels", &channels))
        {
          g_object_set (entry->parse, "num-channels", channels, NULL);
        }

      pcm_format = gst_structure_get_string (gst_struct, "format");
      if (pcm_format)
        {
          g_object_set (entry->parse, "pcm-format",
                        gst_audio_format_from_string (pcm_format), NULL);
        }
    }

  gst_structure_free (gst_struct);

  self->queue = g_slist_append (self->queue, entry);

  if (!self->queue->next)
    {
      g_object_notify (G_OBJECT (self), "speaking");
      _speak_current_entry (self);
    }
}

/**
 * spiel_speaker_get_voices: (get-property voices)
 * @self: a `SpielSpeaker`
 *
 * Gets the voices available to this speaker.
 *
 * Returns: (transfer none): A `GListModel` of `SpielVoice`
 *
 * Since: 1.0
 */
GListModel *
spiel_speaker_get_voices (SpielSpeaker *self)
{
  g_return_val_if_fail (SPIEL_IS_SPEAKER (self), NULL);

  return spiel_registry_get_voices (self->registry);
}

/**
 * spiel_speaker_get_providers: (get-property providers)
 * @self: a `SpielSpeaker`
 *
 * Gets the speech providers that offer voices to this speaker.
 *
 * Returns: (transfer none): A `GListModel` of `SpielProvider`
 *
 * Since: 1.0
 */
GListModel *
spiel_speaker_get_providers (SpielSpeaker *self)
{
  g_return_val_if_fail (SPIEL_IS_SPEAKER (self), NULL);

  return spiel_registry_get_providers (self->registry);
}

/**
 * spiel_speaker_pause:
 * @self: a `SpielSpeaker`
 *
 * Pause the given speaker. If an utterance is being spoken, it will pause
 * until [method@Speaker.resume] is called.
 *
 * If the speaker isn't speaking, calling [method@Speaker.speak] will store
 * new utterances in a queue until [method@Speaker.resume] is called.
 *
 * Since: 1.0
 */
void
spiel_speaker_pause (SpielSpeaker *self)
{
  _QueueEntry *entry = NULL;

  g_return_if_fail (SPIEL_IS_SPEAKER (self));

  if (self->paused)
    {
      return;
    }

  if (self->queue)
    {
      entry = self->queue->data;
    }

  if (!entry)
    {
      self->paused = TRUE;
      g_object_notify (G_OBJECT (self), "paused");
      return;
    }

  gst_element_set_state (self->pipeline, GST_STATE_PAUSED);
}

/**
 * spiel_speaker_resume:
 * @self: a `SpielSpeaker`
 *
 * Resumes the given paused speaker.
 *
 * If the speaker isn't paused this will do nothing.
 *
 * Since: 1.0
 */
void
spiel_speaker_resume (SpielSpeaker *self)
{
  _QueueEntry *entry = NULL;

  g_return_if_fail (SPIEL_IS_SPEAKER (self));

  if (!self->paused)
    {
      return;
    }

  if (self->queue)
    {
      entry = self->queue->data;
    }

  if (!entry)
    {
      self->paused = FALSE;
      g_object_notify (G_OBJECT (self), "paused");
      return;
    }

  gst_element_set_state (self->pipeline, GST_STATE_PLAYING);
}

/**
 * spiel_speaker_cancel:
 * @self: a `SpielSpeaker`
 *
 * Stops the current utterance from being spoken and dumps the utterance queue.
 *
 * Since: 1.0
 */
void
spiel_speaker_cancel (SpielSpeaker *self)
{
  g_return_if_fail (SPIEL_IS_SPEAKER (self));

  if (!self->queue)
    {
      return;
    }

  // Dump the rest of the queue after the current entry.
  g_slist_free_full (g_steal_pointer (&self->queue->next),
                     (GDestroyNotify) _queue_entry_destroy);

  _advance_to_next_entry_or_finish (self, TRUE);
}

static gboolean
_handle_gst_state_change (GstBus *bus, GstMessage *msg, SpielSpeaker *self)
{
  GstState old_state, new_state, pending_state;
  _QueueEntry *entry = self->queue ? self->queue->data : NULL;
  GstObject *element = GST_MESSAGE_SRC (msg);

  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);

  if (new_state == GST_STATE_PLAYING &&
      pending_state == GST_STATE_VOID_PENDING &&
      element == GST_OBJECT (self->pipeline))
    {
      if (self->paused)
        {
          self->paused = FALSE;
          g_object_notify (G_OBJECT (self), "paused");
        }

      if (entry && !entry->started)
        {
          entry->started = TRUE;
          g_signal_emit (self, speaker_signals[UTTURANCE_STARTED], 0,
                         entry->utterance);
          if (entry->deferred_messages)
            {
              g_slist_foreach (entry->deferred_messages,
                               (GFunc) _process_going_to_speak_message, self);
              g_slist_free_full (g_steal_pointer (&entry->deferred_messages),
                                 (GDestroyNotify) gst_message_unref);
            }
        }
    }
  if (new_state == GST_STATE_PAUSED &&
      pending_state == GST_STATE_VOID_PENDING &&
      element == GST_OBJECT (self->pipeline))
    {
      self->paused = TRUE;
      g_object_notify (G_OBJECT (self), "paused");
    }

  if (new_state == GST_STATE_NULL && pending_state == GST_STATE_VOID_PENDING &&
      entry && element == GST_OBJECT (entry->src))
    {
      _advance_to_next_entry_or_finish (self, FALSE);
    }

  return TRUE;
}

static gboolean
_handle_gst_eos (GstBus *bus, GstMessage *msg, SpielSpeaker *self)
{
  _QueueEntry *entry = self->queue ? self->queue->data : NULL;

  g_return_val_if_fail (entry != NULL, TRUE);

  gst_element_set_state (GST_ELEMENT (entry->src), GST_STATE_NULL);

  return TRUE;
}

static void
_process_going_to_speak_message (GstMessage *msg, SpielSpeaker *self)
{
  _QueueEntry *entry = self->queue ? self->queue->data : NULL;
  const GstStructure *strct = gst_message_get_structure (msg);
  guint event_type = SPEECH_PROVIDER_EVENT_TYPE_NONE;
  guint32 range_start = 0;
  guint32 range_end = 0;
  const char *mark_name = gst_structure_get_string (strct, "name");

  g_return_if_fail (entry != NULL);

  if (!gst_structure_get_uint (strct, "event_type", &event_type))
    {
      g_warning ("No 'event_type' in message structure");
    }

  if (!gst_structure_get_uint (strct, "range_start", &range_start))
    {
      g_warning ("No 'range_start' in message structure");
    }

  if (!gst_structure_get_uint (strct, "range_end", &range_end))
    {
      g_warning ("No 'range_end' in message structure");
    }

  switch (event_type)
    {
    case SPEECH_PROVIDER_EVENT_TYPE_WORD:
      g_signal_emit (self, speaker_signals[WORD_STARTED], 0, entry->utterance,
                     range_start, range_end);
      break;
    case SPEECH_PROVIDER_EVENT_TYPE_SENTENCE:
      g_signal_emit (self, speaker_signals[SENTENCE_STARTED], 0,
                     entry->utterance, range_start, range_end);
      break;
    case SPEECH_PROVIDER_EVENT_TYPE_RANGE:
      g_signal_emit (self, speaker_signals[RANGE_STARTED], 0, entry->utterance,
                     range_start, range_end);
      break;
    case SPEECH_PROVIDER_EVENT_TYPE_MARK:
      g_warn_if_fail (mark_name);
      g_signal_emit (self, speaker_signals[MARK_REACHED], 0, entry->utterance,
                     mark_name);
      break;
    default:
      g_warning ("Event type not recognized (%d)", event_type);
      break;
    }
}

static gboolean
_handle_gst_element_message (GstBus *bus, GstMessage *msg, SpielSpeaker *self)
{
  _QueueEntry *entry = self->queue ? self->queue->data : NULL;
  const GstStructure *strct = gst_message_get_structure (msg);

  if (!entry)
    {
      return TRUE;
    }

  if (!strct ||
      !g_str_equal (gst_structure_get_name (strct), "SpielGoingToSpeak"))
    {
      return TRUE;
    }

  if (entry->started)
    {
      _process_going_to_speak_message (msg, self);
    }
  else
    {
      entry->deferred_messages =
          g_slist_append (entry->deferred_messages, gst_message_ref (msg));
    }

  return TRUE;
}

static void
_provider_call_synthesize_done (GObject *source_object,
                                GAsyncResult *res,
                                gpointer user_data)
{
  _CallSynthData *call_synth_data = user_data;
  SpielProviderProxy *provider = SPIEL_PROVIDER_PROXY (source_object);
  GError *err = NULL;
  spiel_provider_proxy_call_synthesize_finish (provider, NULL, res, &err);
  if (err != NULL)
    {
      SpielSpeaker *self = SPIEL_SPEAKER (call_synth_data->self);
      GSList *item = self->queue;
      while (item)
        {
          _QueueEntry *entry = item->data;
          if (entry->utterance == call_synth_data->utterance)
            {
              g_assert (!entry->error);
              entry->error = err;
              if (item == self->queue)
                {
                  // Top of queue failed, advance to next.
                  _advance_to_next_entry_or_finish (call_synth_data->self,
                                                    FALSE);
                }
              break;
            }
          item = item->next;
        }

      if (!item)
        {
          g_error_free (err);
        }
    }

  g_object_unref (call_synth_data->utterance);
  g_slice_free (_CallSynthData, call_synth_data);
}

static void
_speak_current_entry (SpielSpeaker *self)
{
  _QueueEntry *entry = self->queue ? self->queue->data : NULL;

  g_assert (entry);

  if (entry->error)
    {
      g_assert (!entry->src);
      g_assert (!entry->parse);
      g_assert (!entry->volume);
      _advance_to_next_entry_or_finish (self, FALSE);
      return;
    }

  gst_bin_add_many (GST_BIN (self->pipeline), g_object_ref (entry->src),
                    g_object_ref (entry->parse), g_object_ref (entry->volume),
                    NULL);

  gst_element_link_many (entry->src, entry->parse, entry->volume, self->convert,
                         NULL);

  if (!self->paused)
    {
      gst_element_set_state (self->pipeline, GST_STATE_PLAYING);
    }
}

static void
_advance_to_next_entry_or_finish (SpielSpeaker *self, gboolean canceled)
{
  _QueueEntry *entry = self->queue ? self->queue->data : NULL;

  g_assert (entry);

  gst_element_set_state (self->pipeline, GST_STATE_NULL);

  if (entry->error)
    {
      g_signal_emit (self, speaker_signals[UTTERANCE_ERROR], 0,
                     entry->utterance, entry->error);
    }
  else
    {
      g_signal_emit (self,
                     canceled ? speaker_signals[UTTERANCE_CANCELED]
                              : speaker_signals[UTTERANCE_FINISHED],
                     0, entry->utterance);
    }

  if (entry->src)
    {
      g_assert (entry->parse);
      g_assert (entry->volume);
      gst_bin_remove_many (GST_BIN (self->pipeline), entry->src, entry->parse,
                           entry->volume, NULL);
    }

  _queue_entry_destroy (entry);
  self->queue = g_slist_delete_link (self->queue, self->queue);
  if (self->queue)
    {
      _speak_current_entry (self);
    }
  else
    {
      g_object_notify (G_OBJECT (self), "speaking");
    }
}
