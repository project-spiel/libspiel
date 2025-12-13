# SPDX-License-Identifier: LGPL-2.1-or-later
#
# This file is formatted with Python Black

import dbus.service
import dbus
import os
import re
import sys
import json

import gi

gi.require_version("Gst", "1.0")
gi.require_version("SpeechProvider", "1.0")
from gi.repository import GLib, Gst, SpeechProvider

Gst.init(None)

BUS_NAME = "org.one.Speech.Provider"
MAIN_OBJ = "/org/one/Speech/Provider"
PROVIDER_NAME = BUS_NAME.split(".")[1]

try:
    import json

    BUS_NAME = json.loads(sys.argv[-1])["name"]
except:
    pass

SYSTEM_BUS = False
MAIN_IFACE = "org.freedesktop.Speech.Provider"
MOCK_IFACE = "org.freedesktop.Speech.Provider.Mock"
MAIN_OBJ = "/" + BUS_NAME.replace(".", "/")
PROVIDER_NAME = BUS_NAME.split(".")[1]

VERSION = 1

VOICE_SETS = {
    "one": [
        (
            "Chinese (Cantonese)",
            "sit/yue",
            "audio/x-raw,format=S16LE,channels=1,rate=22050",
            SpeechProvider.VoiceFeature.SSML_SAY_AS_CARDINAL
            | SpeechProvider.VoiceFeature.SSML_SAY_AS_ORDINAL,
            ["yue", "zh-yue", "zh"],
        ),
        (
            "Armenian (East Armenia)",
            "ine/hy",
            "audio/x-raw,format=S16LE,channels=1,rate=22050",
            0,
            ["hy", "hy-arevela"],
        ),
    ],
    "two": [
        (
            "Armenian (West Armenia)",
            "ine/hyw",
            "audio/x-raw,format=S16LE,channels=1,rate=22050",
            0,
            ["hyw", "hy-arevmda", "hy"],
        ),
        (
            "English (Scotland)",
            "gmw/en-GB-scotland#misconfigured",
            "nuthin",
            0,
            ["en-gb-scotland", "en"],
        ),
        (
            "English (Lancaster)",
            "gmw/en-GB-x-gbclan",
            "audio/x-raw,format=S16LE,channels=1,rate=22050",
            0,
            ["en-gb-x-gbclan", "en-gb", "en"],
        ),
        (
            "English (America)",
            "gmw/en-US",
            "audio/x-spiel,format=S16LE,channels=1,rate=22050",
            0,
            ["en-us", "en"],
        ),
        (
            "English (Great Britain)",
            "gmw/en",
            "audio/x-raw,format=S16LE,channels=1,rate=22050",
            0,
            ["en-gb", "en"],
        ),
    ],
    "three": [
        (
            "Uzbek",
            "trk/uz",
            "audio/x-spiel,format=S16LE,channels=1,rate=22050",
            0,
            ["uz"],
        )
    ],
}

PROPS = {
    "Name": f"Speech Provider ({PROVIDER_NAME})",
    "Voices": dbus.Array(
        VOICE_SETS[PROVIDER_NAME],
        signature=dbus.Signature("(ssstas)"),
    ),
}


def load(mock, parameters={}):
    mock.AddProperties(MAIN_IFACE, PROPS)
    mock._infinite = False
    mock._last_speak_args = None
    mock._stream = None
    mock.log(f"Loading mock speech provider with parameters: {parameters}")


@dbus.service.method(
    MAIN_IFACE,
    in_signature="hssddbs",
    out_signature="",
)
def Synthesize(self, pipe_fd, text, voice_id, pitch, rate, is_ssml, language):
    if text == "die":
        # special utterance text that makes us die
        exit()
    fd = pipe_fd.take()
    self._last_speak_args = (fd, text, voice_id, pitch, rate, is_ssml, language)
    voice = dict([[v[1], v] for v in PROPS["Voices"]])[voice_id]
    output_format = voice[2]
    synthstream_cls = RawSynthStream
    if output_format.startswith("audio/x-spiel"):
        synthstream_cls = SpielSynthStream
    self._stream = synthstream_cls(fd, text, self._infinite, text == "silent")

    if text != "no data":
        self._stream.start()


@dbus.service.method(
    MOCK_IFACE,
    in_signature="",
    out_signature="",
)
def Hide(self):
    name = self.bus_name.get_name()
    if not name.endswith("_"):
        self.bus_name.get_bus().release_name(name)
        self.bus_name._name = f"{name}_"
        self.bus_name.get_bus().request_name(self.bus_name._name, 0)


@dbus.service.method(
    MOCK_IFACE,
    in_signature="",
    out_signature="",
)
def Show(self):
    name = self.bus_name.get_name()
    if name.endswith("_"):
        self.bus_name.get_bus().release_name(name)
        self.bus_name._name = name[:-1]
        self.bus_name.get_bus().request_name(self.bus_name._name, 0)


@dbus.service.method(
    MOCK_IFACE,
    in_signature="b",
    out_signature="",
)
def SetInfinite(self, val):
    self._infinite = val


@dbus.service.method(
    MOCK_IFACE,
    in_signature="",
    out_signature="",
)
def End(self):
    if self._stream:
        self._stream.end()


@dbus.service.method(
    MOCK_IFACE,
    in_signature="",
    out_signature="",
)
def FlushTasks(self):
    self.stream = None
    self._last_speak_args = None


@dbus.service.method(
    MOCK_IFACE,
    in_signature="",
    out_signature="(tssddbs)",
)
def GetLastSpeakArguments(self):
    if self._last_speak_args is None:
        return (0, "", "", 0.0, 0.0, False, "")
    return self._last_speak_args


def emit_voices_changes(mock_obj):
    mock_obj.UpdateProperties(MAIN_IFACE, {"Voices": PROPS["Voices"]})


@dbus.service.method(
    MOCK_IFACE,
    in_signature="ssstas",
    out_signature="t",
)
def AddVoice(self, name, identifier, output_format, features, languages):
    PROPS["Voices"].append((name, identifier, output_format, features, languages))
    GLib.idle_add(emit_voices_changes, self)
    return len(PROPS["Voices"])


@dbus.service.method(
    MOCK_IFACE,
    in_signature="s",
    out_signature="",
)
def RemoveVoice(self, voice_id):
    for index, v in enumerate(PROPS["Voices"]):
        if v[1] == voice_id:
            PROPS["Voices"].pop(index)
            break
    GLib.idle_add(emit_voices_changes, self)


@dbus.service.method(
    MOCK_IFACE,
    in_signature="",
    out_signature="",
)
def Die(self):
    GLib.idle_add(lambda: exit())


class RawSynthStream(object):
    def __init__(self, fd, text, indefinite, silent):
        elements = [
            "audiotestsrc num-buffers=%d wave=%s name=src"
            % (-1 if indefinite else 10, "silence" if silent else "sine"),
            "audioconvert",
            "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "fdsink name=sink",
        ]
        self._pipeline = Gst.parse_launch(" ! ".join(elements))
        self._pipeline.get_by_name("sink").set_property("fd", fd)
        bus = self._pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect("message::eos", self._on_eos)

    def start(self):
        self._pipeline.set_state(Gst.State.PLAYING)

    def end(self):
        src = self._pipeline.get_by_name("src")
        src.set_state(Gst.State.NULL)
        raw_fd = self._pipeline.get_by_name("sink").get_property("fd")
        os.close(raw_fd)

    def _on_eos(self, *args):
        src = self._pipeline.get_by_name("src")
        raw_fd = self._pipeline.get_by_name("sink").get_property("fd")
        os.close(raw_fd)
        src.set_state(Gst.State.NULL)


class SpielSynthStream(object):
    def __init__(self, fd, text, indefinite, silent):
        self.ranges = [
            [m.start(), m.end()] for m in re.finditer(r".*?\b\W\s?", text, re.MULTILINE)
        ]
        num_buffers = -1
        if not indefinite:
            num_buffers = max(10, len(self.ranges) + 1)
        elements = [
            "audiotestsrc num-buffers=%d wave=%s name=src"
            % (-1 if indefinite else 10, "silence" if silent else "sine"),
            "audioconvert",
            "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "appsink emit-signals=True name=sink",
        ]
        self._pipeline = Gst.parse_launch(" ! ".join(elements))
        sink = self._pipeline.get_by_name("sink")
        sink.connect("new-sample", self._on_new_sample)
        sink.connect("eos", self._on_eos)

        self.stream_writer = SpeechProvider.StreamWriter.new(fd)

    def _on_new_sample(self, sink):
        sample = sink.emit("pull-sample")
        buffer = sample.get_buffer()
        b = buffer.extract_dup(0, buffer.get_size())
        # Some chaos
        self.stream_writer.send_audio(b"")
        if self.ranges:
            start, end = self.ranges.pop(0)
            if len(self.ranges) % 2:
                self.stream_writer.send_event(
                    SpeechProvider.EventType.SENTENCE, start, end, ""
                )
            self.stream_writer.send_event(SpeechProvider.EventType.WORD, start, end, "")
        self.stream_writer.send_audio(b)

        return Gst.FlowReturn.OK

    def end(self):
        self._pipeline.set_state(Gst.State.NULL)
        self.stream_writer.close()

    def start(self):
        self.stream_writer.send_stream_header()
        self._pipeline.set_state(Gst.State.PLAYING)

    def _on_eos(self, *args):
        self.stream_writer.close()
