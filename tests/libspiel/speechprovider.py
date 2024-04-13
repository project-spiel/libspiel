#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright © 2024 GNOME Foundation Inc.
# SPDX-FileContributor: Andy Holmes <andyholmes@gnome.org>

"""SpeechProvider mock template

This creates the expected methods and properties of a SpeechProvider
"""

import os

import dbusmock
import dbus
import gi

gi.require_version("Gst", "1.0")
from gi.repository import GLib, Gst


BUS_NAME = "org.mock.Speech.Provider"
MAIN_IFACE = "org.freedesktop.Speech.Provider"
MAIN_OBJ = "/org/mock/Speech/Provider"
IS_OBJECT_MANAGER = False
SYSTEM_BUS = False


@dbus.service.method(MAIN_IFACE, in_signature="hssddbs", out_signature="")
def Synthesize(self, pipe_fd, _text, _voice_id, _pitch, rate, _is_ssml, _language):
    elements = [
        "audiotestsrc num-buffers=100 wave=sine name=src",
        "audioconvert",
        f"audio/x-raw,format=S16LE,channels=1,rate={rate}",
        "fdsink name=sink",
    ]
    synthesize_pipeline = Gst.parse_launch(" ! ".join(elements))
    synthesize_pipeline.get_by_name("sink").set_property("fd", pipe_fd)
    synthesize_source = synthesize_pipeline.get_by_name("src")

    def _on_eos(self, *args):
        synthesize_source.set_state(Gst.State.NULL)
        os.close(pipe_fd)

    bus = synthesize_pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect("message::eos", _on_eos)

    synthesize_pipeline.set_state(Gst.State.PLAYING)


def load(mock, _parameters):
    mock.AddProperties(
        MAIN_IFACE,
        {
            "Name": "Mock Provider",
            "Voices": dbus.Array(
                [
                    (
                        "Afrikaans",
                        "gmw/af",
                        "audio/x-raw,format=S16LE,channels=1,rate=22050",
                        dbus.UInt64(242123),
                        ["af"],
                    ),
                    (
                        "Cherokee ",
                        "iro/chr",
                        "audio/x-raw,format=S16LE,channels=1,rate=22050",
                        dbus.UInt64(242123),
                        ["chr-US-Qaaa-x-west"],
                    ),
                    (
                        "English (America)",
                        "gmw/en-US",
                        "audio/x-raw,format=S16LE,channels=1,rate=22050",
                        dbus.UInt64(242123),
                        ["en-us", "en"],
                    ),
                ],
                signature="(ssstas)",
            ),
        },
    )
