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
        }
    ],
    "mock3": [
        {
            "name": "English (Great Britain)",
            "identifier": "gmw/en",
            "languages": ["en-gb", "en"],
        },
    ],
}


class SomeObject(dbus.service.Object):
    def __init__(self, *args):
        self._last_speak_args = [0, "", "", 0, 0, 0]
        self._auto_step = not AUTOEXIT
        self._tasks = []
        self._voices = VOICES[NAME][:]
        dbus.service.Object.__init__(self, *args)

    @dbus.service.method(
        "org.freedesktop.Speech.Provider",
        in_signature="tssddd",
        out_signature="",
    )
    def Speak(self, task_id, utterance, voice_id, pitch, rate, volume):
        self._last_speak_args = (task_id, utterance, voice_id, pitch, rate, volume)
        ranges = [[m.start(), m.end()] for m in re.finditer(r".*?\b\W\s?", utterance, re.MULTILINE)]
        self._tasks.append([task_id, ranges])
        GLib.idle_add(lambda: self.SpeechStart(task_id))
        if self._auto_step:
            GLib.idle_add(self._do_step_until_done)

    def _do_step_until_done(self):
        if self._do_step():
            GLib.idle_add(self._do_step_until_done)

    def _do_step(self):
        tasks = self._tasks
        self._tasks = []
        for task in tasks:
            if task[1]:
                start, end = task[1].pop(0)
                self.SpeechRangeStart(task[0], start, end)
                self._tasks.append(task)
            else:
                self.SpeechEnd(task[0])
        return bool(self._tasks)

    @dbus.service.method(
        "org.freedesktop.Speech.Provider",
        in_signature="t",
        out_signature="",
    )
    def Pause(self, task_id):
        return

    @dbus.service.method(
        "org.freedesktop.Speech.Provider",
        in_signature="t",
        out_signature="",
    )
    def Resume(self, task_id):
        return

    @dbus.service.method(
        "org.freedesktop.Speech.Provider",
        in_signature="t",
        out_signature="",
    )
    def Cancel(self, task_id):
        self._tasks = []
        if AUTOEXIT:
            GLib.idle_add(self.byebye)

    @dbus.service.method(
        "org.freedesktop.Speech.Provider",
        in_signature="",
        out_signature="a(ssas)",
    )
    def GetVoices(self):
        if AUTOEXIT:
            GLib.idle_add(self.byebye)
        return [(v["name"], v["identifier"], v["languages"]) for v in self._voices]

    @dbus.service.signal("org.freedesktop.Speech.Provider", signature="t")
    def SpeechStart(self, task_id):
        pass

    @dbus.service.signal("org.freedesktop.Speech.Provider", signature="ttt")
    def SpeechRangeStart(self, task_id, start, end):
        pass

    @dbus.service.signal("org.freedesktop.Speech.Provider", signature="t")
    def SpeechEnd(self, task_id):
        if AUTOEXIT:
            GLib.idle_add(self.byebye)

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
        if self._auto_step:
            GLib.idle_add(self._do_step_until_done)

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

    def byebye(self):
        exit()


if __name__ == "__main__":
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    session_bus = dbus.SessionBus()
    name = dbus.service.BusName(f"org.{NAME}.Speech.Provider", session_bus)
    obj = SomeObject(session_bus, f"/org/{NAME}/Speech/Provider")

    mainloop = GLib.MainLoop()
    mainloop.run()
