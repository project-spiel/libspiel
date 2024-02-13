/* speech-provider-stream-reader.h
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define SPEECH_PROVIDER_TYPE_STREAM_READER                                     \
  (speech_provider_stream_reader_get_type ())

G_DECLARE_FINAL_TYPE (SpeechProviderStreamReader,
                      speech_provider_stream_reader,
                      SPEECH_PROVIDER,
                      STREAM_READER,
                      GObject)

SpeechProviderStreamReader *speech_provider_stream_reader_new (gint fd);

void speech_provider_stream_reader_close (SpeechProviderStreamReader *self);

gboolean speech_provider_stream_reader_get_stream_header (
    SpeechProviderStreamReader *self);

gboolean speech_provider_stream_reader_get_audio (
    SpeechProviderStreamReader *self, guint8 **chunk, guint32 *chunk_size);

gboolean
speech_provider_stream_reader_get_event (SpeechProviderStreamReader *self,
                                         SpeechProviderEventType *event_type,
                                         guint32 *range_start,
                                         guint32 *range_end,
                                         char **mark_name);

G_END_DECLS
