#!/usr/bin/env python3

from gi.repository import GLib

import dbus
import dbus.service
import dbus.mainloop.glib
import re
from os import getcwd
from sys import argv

NAME = argv[-1] if len(argv) > 1 else "mock"

AUTOEXIT = NAME == "mock3"

VOICES = {
    "mock": [
        {
            "name": "Chinese (Cantonese)",
            "identifier": "sit/yue",
            "languages": ["yue", "zh-yue", "zh"],
        },
        {
            "name": "Armenian (East Armenia)",
            "identifier": "ine/hy",
            "languages": ["hy", "hy-arevela"],
        },
    ],
    "mock2": [
        {
            "name": "Armenian (West Armenia)",
            "identifier": "ine/hyw",
            "languages": ["hyw", "hy-arevmda", "hy"],
        },
        {
            "name": "English (Scotland)",
            "identifier": "gmw/en-GB-scotland",
            "languages": ["en-gb-scotland", "en"],
        },
        {
            "name": "English (Lancaster)",
            "identifier": "gmw/en-GB-x-gbclan",
            "languages": ["en-gb-x-gbclan", "en-gb", "en"],
        },
        {
            "name": "English (America)",
            "identifier": "gmw/en-US",
            "languages": ["en-us", "en"],
        },
        {
            "name": "English (Great Britain)",
            "identifier": "gmw/en",
            "languages": ["en-gb", "en"],
        },
    ],
    "mock3": [
        {
            "name": "Uzbek",
            "identifier": "trk/uz",
            "languages": ["uz"],
        },
    ],
}


class SomeObject(dbus.service.Object):
    def __init__(self, *args):
        self._last_speak_args = [0, "", "", 0, 0, 0]
        self._auto_step = not AUTOEXIT
        self._tasks = []
        self._voices = VOICES[NAME][:]
        self._die_on_speak = False
        dbus.service.Object.__init__(self, *args)

    @dbus.service.method(
        "org.freedesktop.Speech.Provider",
        in_signature="",
        out_signature="a(ssas)",
    )
    def GetVoices(self):
        if AUTOEXIT:
            GLib.idle_add(self.byebye)
        return [(v["name"], v["identifier"], v["languages"]) for v in self._voices]

    @dbus.service.signal("org.freedesktop.Speech.Provider")
    def VoicesChanged(self):
        pass

    @dbus.service.method(
        "org.freedesktop.Speech.MockProvider",
        in_signature="",
        out_signature="tssddd",
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
    def SetAutoStep(self, val):
        self._auto_step = val

    @dbus.service.method(
        "org.freedesktop.Speech.MockProvider",
        in_signature="ssas",
        out_signature="",
    )
    def AddVoice(self, name, identifier, languages):
        self._voices.append(
            {"name": name, "identifier": identifier, "languages": languages}
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
    def DieOnSpeak(self):
        self._die_on_speak = True

    def byebye(self):
        exit()


if __name__ == "__main__":
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    session_bus = dbus.SessionBus()
    name = dbus.service.BusName(f"org.{NAME}.Speech.Provider", session_bus)
    obj = SomeObject(session_bus, f"/org/{NAME}/Speech/Provider")

    mainloop = GLib.MainLoop()
    mainloop.run()
