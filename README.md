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
