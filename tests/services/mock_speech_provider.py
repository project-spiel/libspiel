#!/usr/bin/env python3

import gi

gi.require_version("Gst", "1.0")
from gi.repository import GLib, Gst

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
            "languages": ["yue", "zh-yue", "zh"],
        },
        {
            "name": "Armenian (East Armenia)",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "ine/hy",
            "languages": ["hy", "hy-arevela"],
        },
    ],
    "mock2": [
        {
            "name": "Armenian (West Armenia)",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "ine/hyw",
            "languages": ["hyw", "hy-arevmda", "hy"],
        },
        {
            "name": "English (Scotland)",
            "output_format": "nuthin",
            "identifier": "gmw/en-GB-scotland",
            "languages": ["en-gb-scotland", "en"],
        },
        {
            "name": "English (Lancaster)",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "gmw/en-GB-x-gbclan",
            "languages": ["en-gb-x-gbclan", "en-gb", "en"],
        },
        {
            "name": "English (America)",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "gmw/en-US",
            "languages": ["en-us", "en"],
        },
        {
            "name": "English (Great Britain)",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "gmw/en",
            "languages": ["en-gb", "en"],
        },
    ],
    "mock3": [
        {
            "name": "Uzbek",
            "output_format": "audio/x-raw,format=S16LE,channels=1,rate=22050",
            "identifier": "trk/uz",
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

    def _on_eos(self, *args):
        src = self._pipeline.get_by_name("src")
        raw_fd = self._pipeline.get_by_name("sink").get_property("fd")
        os.close(raw_fd)
        src.set_state(Gst.State.NULL)


class SomeObject(dbus.service.Object):
    def __init__(self, *args):
        self._last_speak_args = [0, "", "", 0, 0, 0]
        self._infinite = False
        self._tasks = []
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
        stream = RawSynthStream(raw_fd, utterance, self._infinite)
        stream.start()

    @dbus.service.method(
        "org.freedesktop.Speech.Provider",
        in_signature="",
        out_signature="a(sssas)",
    )
    def GetVoices(self):
        if AUTOEXIT:
            GLib.idle_add(self.byebye)
        return [
            (v["name"], v["identifier"], v["output_format"], v["languages"])
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
    def Step(self):
        self._do_step()

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
