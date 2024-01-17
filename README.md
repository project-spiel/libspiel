<img alt="libspiel logo" align="right" src="https://raw.githubusercontent.com/eeejay/libspiel/main/spiel-logo.svg">

# Spiel

[![ Build, Test & Docs](https://github.com/eeejay/libspiel/actions/workflows/ci.yml/badge.svg)](https://github.com/eeejay/libspiel/actions/workflows/ci.yml)

## Overview

This project provides a speech synthesis API for desktop Linux and beyond. It consists of two parts, a speech provider interface specification and a client library.

### org.freedesktop.Speech.Provider
This is a [loose specification](https://eeejay.github.io/libspiel/generated-org.freedesktop.Speech.Provider.html) of what methods and signals a D-Bus speech provider should implement in order to be discoverable and usable by client applications. It consists of simple speech methods such as `Speak()`, `Pause()` and `Cancel()`, and speech events to notify of state changes of spoken text. There is also a `GetVoices()` method that should return a list of available voices and their associated properties (like supported language).

The interface for a typical speech provider implementation could be written in a couple hundred lines of code. The specification is designed to be simple and straightforward. The really neat thing about this kind of speech provider is that it can be distributed as a Flatpak or Snap without any system prerequisites and be completely self contained.


### Client Library (libspiel)
The [libspiel client library](https://eeejay.github.io/libspiel/) is designed to provide an ergonomic interface to the myriad of potential speech providers that are installed in a given session. The API is inspired by the W3C Web Speech API. It serves several purposes:
* Provide an updated list of installed across all speech providers voices.
* Offer a “speaker” abstraction where utterances can be queued to speak.
* If no voice was explicitly chosen for an utterance, negotiate global user settings and language preferences to choose the most appropriate voice.

Language bindings are available through GObject Introspection. So this should work for any application, be it in C/C++, Python, Rust, ECMAscript, or Lua.

A minimal python example would look like this:
```python
import gi
gi.require_version("Spiel", "0.1")
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

We use the Meson build system for building libspiel.

```sh
# Some common options
meson setup build
meson compile -C build
```

## Documentation

There is [auto-generated API reference](https://eeejay.github.io/libspiel/) along with a [documented D-Bus interface](https://eeejay.github.io/libspiel/generated-org.freedesktop.Speech.Provider.html) for speech providers.