# libspiel

[![ Build & Test ](https://github.com/project-spiel/libspiel/actions/workflows/ci.yml/badge.svg)](https://github.com/project-spiel/libspiel/actions/workflows/ci.yml) [![ Docs & Website ](https://github.com/project-spiel/libspiel/actions/workflows/website.yml/badge.svg)](https://github.com/project-spiel/libspiel/actions/workflows/website.yml)

## Overview

This client library is designed to provide an ergonomic interface to the myriad of potential speech providers that are installed in a given session. The API is inspired by the W3C Web Speech API. It serves several purposes:
* Provide an updated list of installed across all speech providers voices.
* Offer a “speaker” abstraction where utterances can be queued to speak.
* If no voice was explicitly chosen for an utterance, negotiate global user settings and language preferences to choose the most appropriate voice.

Language bindings are available through GObject Introspection. So this should work for any application, be it in C/C++, Python, Rust, ECMAscript, or Lua.

A minimal python example would look like this:
```python
import gi
gi.require_version("Spiel", "1.0")
from gi.repository import GLib, Spiel

loop = GLib.MainLoop()

def _notify_speaking_cb(synth, val):
    if not synth.props.speaking:
        loop.quit()

speaker = Spiel.Speaker.new_sync(None)
speaker.connect("notify::speaking", _notify_speaking_cb)

utterance = Spiel.Utterance(text="Hello world.")
speaker.speak(utterance)

loop.run()

```

## Building

We use the Meson build system for building Spiel.

```sh
# Some common options
meson setup build
meson compile -C build
```

## Documentation

There is an [auto-generated API reference](https://project-spiel.org/libspiel/).