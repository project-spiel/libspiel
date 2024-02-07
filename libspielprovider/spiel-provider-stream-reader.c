/* spiel-provider-stream-reader.c
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

#include "spiel-provider.h"

#include "spiel-provider-stream-reader.h"

#include <fcntl.h>
#include <unistd.h>

struct _SpielProviderStreamReader
{
  GObject parent_instance;
};

typedef struct
{
  gint fd;
  gboolean stream_header_recieved;
  SpielProviderChunkType next_chunk_type;
} SpielProviderStreamReaderPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (SpielProviderStreamReader,
                                  spiel_provider_stream_reader,
                                  G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_FD,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/**
 * spiel_provider_stream_reader_new: (constructor)
 * @fd: The file descriptor for a pipe
 *
 * Creates a new #SpielProviderStreamReader.
 *
 * Returns: The new #SpielProviderStreamReader.
 */
SpielProviderStreamReader *
spiel_provider_stream_reader_new (gint fd)
{
  if (fcntl (fd, F_GETFD) == -1)
    {
      g_warning ("Bad file descriptor");
      return NULL;
    }

  return g_object_new (SPIEL_PROVIDER_TYPE_STREAM_READER, "fd", fd, NULL);
}

/**
 * spiel_provider_stream_reader_close:
 *
 * Close the pipe.
 *
 */
void
spiel_provider_stream_reader_close (SpielProviderStreamReader *self)
{
  SpielProviderStreamReaderPrivate *priv =
      spiel_provider_stream_reader_get_instance_private (self);
  close (priv->fd);
  priv->fd = -1;
}

/**
 * spiel_provider_stream_reader_get_stream_header:
 *
 * Retrieves stream header.
 *
 * Returns: %TRUE if header successfully recieved.
 */
gboolean
spiel_provider_stream_reader_get_stream_header (SpielProviderStreamReader *self)
{
  SpielProviderStreamReaderPrivate *priv =
      spiel_provider_stream_reader_get_instance_private (self);
  SpielProviderStreamHeader header;
  g_assert (!priv->stream_header_recieved);
  read (priv->fd, &header, sizeof (SpielProviderStreamHeader));
  priv->stream_header_recieved = TRUE;
  return g_str_equal (header.version, SPIEL_PROVIDER_STREAM_PROTOCOL_VERSION);
}

static SpielProviderChunkType
_get_next_chunk_type (SpielProviderStreamReader *self)
{
  SpielProviderStreamReaderPrivate *priv =
      spiel_provider_stream_reader_get_instance_private (self);
  if (priv->next_chunk_type == SPIEL_PROVIDER_CHUNK_TYPE_NONE)
    {
      read (priv->fd, &priv->next_chunk_type, sizeof (SpielProviderChunkType));
    }
  return priv->next_chunk_type;
}

/**
 * spiel_provider_stream_reader_get_audio:
 * @chunk: (out) (array length=chunk_size) (transfer full): Location to
 *        store audio data
 * @chunk_size: (out): Location to store size of chunk
 *
 * Retrieves audio data
 *
 * Returns: (skip): %TRUE if the call succeeds.
 */
gboolean
spiel_provider_stream_reader_get_audio (SpielProviderStreamReader *self,
                                        guint8 **chunk,
                                        guint32 *chunk_size)
{
  SpielProviderStreamReaderPrivate *priv =
      spiel_provider_stream_reader_get_instance_private (self);
  SpielProviderChunkType chunk_type = _get_next_chunk_type (self);
  g_assert (priv->stream_header_recieved);
  if (chunk_type != SPIEL_PROVIDER_CHUNK_TYPE_AUDIO)
    {
      *chunk_size = 0;
      return FALSE;
    }

  read (priv->fd, chunk_size, sizeof (guint32));
  *chunk = g_malloc (*chunk_size * sizeof (guint8));
  read (priv->fd, *chunk, *chunk_size * sizeof (guint8));
  priv->next_chunk_type = SPIEL_PROVIDER_CHUNK_TYPE_NONE;
  return TRUE;
}

/**
 * spiel_provider_stream_reader_get_event:
 * @event_type: (out): type of event
 * @range_start: (out): text range start
 * @range_end: (out): text range end
 * @mark_name: (out): mark name
 *
 * Retrieves event data
 *
 * Returns: (skip): %TRUE if the call succeeds.
 */
gboolean
spiel_provider_stream_reader_get_event (SpielProviderStreamReader *self,
                                        SpielProviderEventType *event_type,
                                        guint32 *range_start,
                                        guint32 *range_end,
                                        char **mark_name)
{
  SpielProviderStreamReaderPrivate *priv =
      spiel_provider_stream_reader_get_instance_private (self);
  SpielProviderChunkType chunk_type = _get_next_chunk_type (self);
  SpielProviderEventData event_data;
  g_assert (priv->stream_header_recieved);
  if (chunk_type != SPIEL_PROVIDER_CHUNK_TYPE_EVENT)
    {
      *event_type = SPIEL_PROVIDER_EVENT_TYPE_NONE;
      return FALSE;
    }
  read (priv->fd, &event_data, sizeof (SpielProviderEventData));
  *event_type = event_data.event_type;
  *range_start = event_data.range_start;
  *range_end = event_data.range_end;
  *mark_name = NULL;
  if (event_data.mark_name_length)
    {
      char *name = g_malloc0 (event_data.mark_name_length * sizeof (char) + 1);
      read (priv->fd, name, event_data.mark_name_length * sizeof (char));
      *mark_name = name;
    }
  priv->next_chunk_type = SPIEL_PROVIDER_CHUNK_TYPE_NONE;
  return TRUE;
}

static void
spiel_provider_stream_reader_finalize (GObject *object)
{
  SpielProviderStreamReader *self = (SpielProviderStreamReader *) object;
  SpielProviderStreamReaderPrivate *priv =
      spiel_provider_stream_reader_get_instance_private (self);

  close (priv->fd);
  G_OBJECT_CLASS (spiel_provider_stream_reader_parent_class)->finalize (object);
}

static void
spiel_provider_stream_reader_get_property (GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
  SpielProviderStreamReader *self = SPIEL_PROVIDER_STREAM_READER (object);
  SpielProviderStreamReaderPrivate *priv =
      spiel_provider_stream_reader_get_instance_private (self);

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
spiel_provider_stream_reader_set_property (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
  SpielProviderStreamReader *self = SPIEL_PROVIDER_STREAM_READER (object);
  SpielProviderStreamReaderPrivate *priv =
      spiel_provider_stream_reader_get_instance_private (self);

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
spiel_provider_stream_reader_class_init (SpielProviderStreamReaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_provider_stream_reader_finalize;
  object_class->get_property = spiel_provider_stream_reader_get_property;
  object_class->set_property = spiel_provider_stream_reader_set_property;

  /**
   * SpielProviderStreamReader:fd:
   *
   * File descriptor for pipe
   *
   */
  properties[PROP_FD] =
      g_param_spec_int ("fd", NULL, NULL, -1, G_MAXINT32, 0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
spiel_provider_stream_reader_init (SpielProviderStreamReader *self)
{
  SpielProviderStreamReaderPrivate *priv =
      spiel_provider_stream_reader_get_instance_private (self);
  priv->stream_header_recieved = FALSE;
  priv->next_chunk_type = SPIEL_PROVIDER_CHUNK_TYPE_NONE;
}