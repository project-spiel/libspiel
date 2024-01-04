<img alt="libspiel logo" align="right" src="https://raw.githubusercontent.com/eeejay/libspiel/main/spiel-logo.svg">

# libspiel

[![ Build, Test & Docs](https://github.com/eeejay/libspiel/actions/workflows/ci.yml/badge.svg)](https://github.com/eeejay/libspiel/actions/workflows/ci.yml)

libspiel is a library that interfaces with D-Bus speech synthesis providers.
The library makes it easy to query for voices and manage a speech utterance queue.

## Building

We use the Meson build system for building libspiel.

```sh
# Some common options
meson setup build
cd build
ninja install
```

## Documentation

There is [auto-generated API reference](https://eeejay.github.io/libspiel/) along with a [documented D-Bus interface](https://eeejay.github.io/libspiel/generated-org.freedesktop.Speech.Provider.html) for speech providers.