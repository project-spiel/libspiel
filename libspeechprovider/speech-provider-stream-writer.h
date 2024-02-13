/* speech-provider-stream-writer.h
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

#define SPEECH_PROVIDER_TYPE_STREAM_WRITER                                     \
  (speech_provider_stream_writer_get_type ())

G_DECLARE_FINAL_TYPE (SpeechProviderStreamWriter,
                      speech_provider_stream_writer,
                      SPEECH_PROVIDER,
                      STREAM_WRITER,
                      GObject)

SpeechProviderStreamWriter *speech_provider_stream_writer_new (gint fd);

void speech_provider_stream_writer_close (SpeechProviderStreamWriter *self);

void speech_provider_stream_writer_send_stream_header (
    SpeechProviderStreamWriter *self);

void speech_provider_stream_writer_send_audio (SpeechProviderStreamWriter *self,
                                               guint8 *chunk,
                                               guint32 chunk_size);

void
speech_provider_stream_writer_send_event (SpeechProviderStreamWriter *self,
                                          SpeechProviderEventType event_type,
                                          guint32 range_start,
                                          guint32 range_end,
                                          const char *mark_name);

G_END_DECLS
