#!/usr/bin/env python3

import gi

gi.require_version("Gst", "1.0")
gi.require_version("SpielProvider", "0.1")
from gi.repository import GLib, Gst, SpielProvider

import dbus
import dbus.service
import dbus.mainloop.glib
import re
import os
from os import getcwd
from sys import argv

Gst.init(None)

NAME = argv[-1] if len(argv) > 1 else "mock"

AUTOEXIT = NAME == "mock3"

VOICES = {
    "mock": [
        {
            "name": "Chinese (Cantonese)",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "sit/yue",
            "features": SpielProvider.VoiceFeature.SSML_SAY_AS_CARDINAL
            | SpielProvider.VoiceFeature.SSML_SAY_AS_ORDINAL,
            "languages": ["yue", "zh-yue", "zh"],
        },
        {
            "name": "Armenian (East Armenia)",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "ine/hy",
            "features": 0,
            "languages": ["hy", "hy-arevela"],
        },
    ],
    "mock2": [
        {
            "name": "Armenian (West Armenia)",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "ine/hyw",
            "features": 0,
            "languages": ["hyw", "hy-arevmda", "hy"],
        },
        {
            "name": "English (Scotland)",
            "output_format": "nuthin",
            "identifier": "gmw/en-GB-scotland#misconfigured",
            "features": 0,
            "languages": ["en-gb-scotland", "en"],
        },
        {
            "name": "English (Lancaster)",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "gmw/en-GB-x-gbclan",
            "features": 0,
            "languages": ["en-gb-x-gbclan", "en-gb", "en"],
        },
        {
            "name": "English (America)",
            "output_format": "audio/x-spiel,format=S16LE,channels=1,rate=22050",
            "identifier": "gmw/en-US",
            "features": 0,
            "languages": ["en-us", "en"],
        },
        {
            "name": "English (Great Britain)",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "gmw/en",
            "features": 0,
            "languages": ["en-gb", "en"],
        },
    ],
    "mock3": [
        {
            "name": "Uzbek",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "trk/uz",
            "features": 0,
            "languages": ["uz"],
        },
    ],
}


class RawSynthStream(object):
    def __init__(self, fd, text, indefinite=False):
        elements = [
            "audiotestsrc num-buffers=%d name=src" % (-1 if indefinite else 10),
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
    def __init__(self, fd, text, indefinite=False):
        self.ranges = ranges = [
            [m.start(), m.end()] for m in re.finditer(r".*?\b\W\s?", text, re.MULTILINE)
        ]
        num_buffers = -1
        if not indefinite:
            num_buffers = max(10, len(self.ranges) + 1)
        elements = [
            f"audiotestsrc num-buffers={num_buffers} name=src",
            "audioconvert",
            "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "appsink emit-signals=True name=sink",
        ]
        self._pipeline = Gst.parse_launch(" ! ".join(elements))
        sink = self._pipeline.get_by_name("sink")
        sink.connect("new-sample", self._on_new_sample)
        sink.connect("eos", self._on_eos)

        self.stream_writer = SpielProvider.StreamWriter.new(fd)

    def _on_new_sample(self, sink):
        sample = sink.emit("pull-sample")
        buffer = sample.get_buffer()
        b = buffer.extract_dup(0, buffer.get_size())
        if self.ranges:
            start, end = self.ranges.pop(0)
            self.stream_writer.send_event(SpielProvider.EventType.WORD, start, end, "")
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


class SomeObject(dbus.service.Object):
    def __init__(self, *args):
        self._last_speak_args = [0, "", "", 0, 0, 0]
        self._infinite = False
        self._stream = None
        self._voices = VOICES[NAME][:]

        dbus.service.Object.__init__(self, *args)

    @dbus.service.method(
        "org.freedesktop.Speech.Provider",
        in_signature="hssdd",
        out_signature="",
    )
    def Synthesize(self, fd, utterance, voice_id, pitch, rate):
        raw_fd = fd.take()
        self._last_speak_args = (raw_fd, utterance, voice_id, pitch, rate)
        voice = dict([[v["identifier"], v] for v in self._voices])[voice_id]
        output_format = voice["output_format"]
        synthstream_cls = RawSynthStream
        if output_format.startswith("audio/x-spiel"):
            synthstream_cls = SpielSynthStream

        self.stream = synthstream_cls(raw_fd, utterance, self._infinite)
        self.stream.start()

    @dbus.service.method(
        "org.freedesktop.Speech.Provider",
        in_signature="",
        out_signature="a(ssstas)",
    )
    def GetVoices(self):
        if AUTOEXIT:
            GLib.idle_add(self.byebye)
        return [
            (
                v["name"],
                v["identifier"],
                v["output_format"],
                v["features"],
                v["languages"],
            )
            for v in self._voices
        ]

    @dbus.service.signal("org.freedesktop.Speech.Provider")
    def VoicesChanged(self):
        pass

    @dbus.service.method(
        "org.freedesktop.Speech.MockProvider",
        in_signature="",
        out_signature="tssdd",
    )
    def GetLastSpeakArguments(self):
        return self._last_speak_args

    @dbus.service.method(
        "org.freedesktop.Speech.MockProvider",
        in_signature="",
        out_signature="",
    )
    def FlushTasks(self):
        self.stream = None
        self._last_speak_args = [0, "", "", 0, 0, 0]

    @dbus.service.method(
        "org.freedesktop.Speech.MockProvider",
        in_signature="b",
        out_signature="",
    )
    def SetInfinite(self, val):
        self._infinite = val

    @dbus.service.method(
        "org.freedesktop.Speech.MockProvider",
        in_signature="",
        out_signature="",
    )
    def End(self):
        if self.stream:
            self.stream.end()

    @dbus.service.method(
        "org.freedesktop.Speech.MockProvider",
        in_signature="ssas",
        out_signature="",
    )
    def AddVoice(self, name, identifier, languages):
        self._voices.append(
            {
                "name": name,
                "identifier": identifier,
                "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
                "features": 0,
                "languages": languages,
            }
        )
        GLib.idle_add(self.VoicesChanged)

    @dbus.service.method(
        "org.freedesktop.Speech.MockProvider",
        in_signature="s",
        out_signature="",
    )
    def RemoveVoice(self, identifier):
        self._voices = [v for v in self._voices if v["identifier"] != identifier]
        GLib.idle_add(self.VoicesChanged)

    @dbus.service.method(
        "org.freedesktop.Speech.MockProvider",
        in_signature="",
        out_signature="",
    )
    def Die(self):
        GLib.idle_add(self.byebye)

    def byebye(self):
        exit()


if __name__ == "__main__":
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    session_bus = dbus.SessionBus()
    name = dbus.service.BusName(f"org.{NAME}.Speech.Provider", session_bus)
    obj = SomeObject(session_bus, f"/org/{NAME}/Speech/Provider")

    mainloop = GLib.MainLoop()
    mainloop.run()
