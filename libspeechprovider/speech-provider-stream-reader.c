/* speech-provider-stream-reader.c
 *
 * Copyright (C) 2024 Eitan Isaacson <eitan@monotonous.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "speech-provider.h"

#include "speech-provider-stream-reader.h"

#include <fcntl.h>
#include <unistd.h>

/**
 * SpeechProviderStreamReader:
 *
 * A provider audio stream reader.
 *
 * Since: 1.0
 */
struct _SpeechProviderStreamReader
{
  GObject parent_instance;
};

typedef struct
{
  gint fd;
  gboolean stream_header_received;
  SpeechProviderChunkType next_chunk_type;
} SpeechProviderStreamReaderPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (SpeechProviderStreamReader,
                                  speech_provider_stream_reader,
                                  G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_FD,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/**
 * speech_provider_stream_reader_new: (constructor)
 * @fd: The file descriptor for a pipe
 *
 * Creates a new [class@SpeechProvider.StreamReader].
 *
 * Returns: (transfer full): The new `SpeechProviderStreamReader`.
 *
 * Since: 1.0
 */
SpeechProviderStreamReader *
speech_provider_stream_reader_new (gint fd)
{
  if (fcntl (fd, F_GETFD) == -1)
    {
      g_warning ("Bad file descriptor");
      return NULL;
    }

  return g_object_new (SPEECH_PROVIDER_TYPE_STREAM_READER, "fd", fd, NULL);
}

/**
 * speech_provider_stream_reader_close:
 * @self: `SpeechProviderStreamReader`
 *
 * Close the pipe.
 *
 * Since: 1.0
 */
void
speech_provider_stream_reader_close (SpeechProviderStreamReader *self)
{
  SpeechProviderStreamReaderPrivate *priv =
      speech_provider_stream_reader_get_instance_private (self);

  g_return_if_fail (SPEECH_PROVIDER_IS_STREAM_READER (self));

  close (priv->fd);
  priv->fd = -1;
}

/**
 * speech_provider_stream_reader_get_stream_header:
 * @self: `SpeechProviderStreamReader`
 *
 * Retrieves stream header.
 *
 * Returns: %TRUE if header successfully received.
 *
 * Since: 1.0
 */
gboolean
speech_provider_stream_reader_get_stream_header (
    SpeechProviderStreamReader *self)
{
  SpeechProviderStreamReaderPrivate *priv =
      speech_provider_stream_reader_get_instance_private (self);
  SpeechProviderStreamHeader header;

  g_return_val_if_fail (SPEECH_PROVIDER_IS_STREAM_READER (self), FALSE);
  g_assert (!priv->stream_header_received);

  read (priv->fd, &header, sizeof (SpeechProviderStreamHeader));
  priv->stream_header_received = TRUE;
  return strncmp (header.version, SPEECH_PROVIDER_STREAM_PROTOCOL_VERSION, 4) ==
         0;
}

static SpeechProviderChunkType
_get_next_chunk_type (SpeechProviderStreamReader *self)
{
  SpeechProviderStreamReaderPrivate *priv =
      speech_provider_stream_reader_get_instance_private (self);
  if (priv->next_chunk_type == SPEECH_PROVIDER_CHUNK_TYPE_NONE)
    {
      read (priv->fd, &priv->next_chunk_type, sizeof (SpeechProviderChunkType));
    }
  return priv->next_chunk_type;
}

/**
 * speech_provider_stream_reader_get_audio:
 * @self: `SpeechProviderStreamReader`
 * @chunk: (out) (array length=chunk_size) (transfer full) (not nullable):
 *        Location to store audio data
 * @chunk_size: (out) (not nullable): Location to store size of chunk
 *
 * Retrieves audio data
 *
 * Returns: (skip): %TRUE if the call succeeds.
 *
 * Since: 1.0
 */
gboolean
speech_provider_stream_reader_get_audio (SpeechProviderStreamReader *self,
                                         guint8 **chunk,
                                         guint32 *chunk_size)
{
  SpeechProviderStreamReaderPrivate *priv =
      speech_provider_stream_reader_get_instance_private (self);
  SpeechProviderChunkType chunk_type = SPEECH_PROVIDER_CHUNK_TYPE_NONE;

  g_return_val_if_fail (SPEECH_PROVIDER_IS_STREAM_READER (self), FALSE);
  g_return_val_if_fail (chunk != NULL && *chunk == NULL, FALSE);
  g_return_val_if_fail (chunk_size != NULL, FALSE);

  chunk_type = _get_next_chunk_type (self);
  g_assert (priv->stream_header_received);
  if (chunk_type != SPEECH_PROVIDER_CHUNK_TYPE_AUDIO)
    {
      *chunk_size = 0;
      return FALSE;
    }

  read (priv->fd, chunk_size, sizeof (guint32));
  *chunk = g_malloc (*chunk_size * sizeof (guint8));
  read (priv->fd, *chunk, *chunk_size * sizeof (guint8));
  priv->next_chunk_type = SPEECH_PROVIDER_CHUNK_TYPE_NONE;
  return TRUE;
}

/**
 * speech_provider_stream_reader_get_event:
 * @self: a `SpeechProviderStreamReader`
 * @event_type: (out) (not nullable): type of event
 * @range_start: (out) (not nullable): text range start
 * @range_end: (out) (not nullable): text range end
 * @mark_name: (out) (not nullable): mark name
 *
 * Retrieves event data
 *
 * Returns: (skip): %TRUE if the call succeeds.
 *
 * Since: 1.0
 */
gboolean
speech_provider_stream_reader_get_event (SpeechProviderStreamReader *self,
                                         SpeechProviderEventType *event_type,
                                         guint32 *range_start,
                                         guint32 *range_end,
                                         char **mark_name)
{
  SpeechProviderStreamReaderPrivate *priv =
      speech_provider_stream_reader_get_instance_private (self);
  SpeechProviderChunkType chunk_type = _get_next_chunk_type (self);
  SpeechProviderEventData event_data;

  g_return_val_if_fail (SPEECH_PROVIDER_IS_STREAM_READER (self), FALSE);
  g_return_val_if_fail (event_type != NULL, FALSE);
  g_return_val_if_fail (range_start != NULL, FALSE);
  g_return_val_if_fail (range_end != NULL, FALSE);
  g_return_val_if_fail (mark_name != NULL && *mark_name == NULL, FALSE);
  g_assert (priv->stream_header_received);

  if (chunk_type != SPEECH_PROVIDER_CHUNK_TYPE_EVENT)
    {
      *event_type = SPEECH_PROVIDER_EVENT_TYPE_NONE;
      return FALSE;
    }
  read (priv->fd, &event_data, sizeof (SpeechProviderEventData));
  *event_type = event_data.event_type;
  *range_start = event_data.range_start;
  *range_end = event_data.range_end;
  if (event_data.mark_name_length)
    {
      char *name = g_malloc0 (event_data.mark_name_length * sizeof (char) + 1);
      read (priv->fd, name, event_data.mark_name_length * sizeof (char));
      *mark_name = name;
    }
  priv->next_chunk_type = SPEECH_PROVIDER_CHUNK_TYPE_NONE;
  return TRUE;
}

static void
speech_provider_stream_reader_get_property (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec)
{
  SpeechProviderStreamReader *self = SPEECH_PROVIDER_STREAM_READER (object);
  SpeechProviderStreamReaderPrivate *priv =
      speech_provider_stream_reader_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_FD:
      g_value_set_int (value, priv->fd);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
speech_provider_stream_reader_set_property (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
  SpeechProviderStreamReader *self = SPEECH_PROVIDER_STREAM_READER (object);
  SpeechProviderStreamReaderPrivate *priv =
      speech_provider_stream_reader_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_FD:
      priv->fd = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
speech_provider_stream_reader_class_init (
    SpeechProviderStreamReaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = speech_provider_stream_reader_get_property;
  object_class->set_property = speech_provider_stream_reader_set_property;

  /**
   * SpeechProviderStreamReader:fd:
   *
   * File descriptor for pipe.
   *
   * Since: 1.0
   */
  properties[PROP_FD] =
      g_param_spec_int ("fd", NULL, NULL, -1, G_MAXINT32, 0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
speech_provider_stream_reader_init (SpeechProviderStreamReader *self)
{
  SpeechProviderStreamReaderPrivate *priv =
      speech_provider_stream_reader_get_instance_private (self);
  priv->stream_header_received = FALSE;
  priv->next_chunk_type = SPEECH_PROVIDER_CHUNK_TYPE_NONE;
}
