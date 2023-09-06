# libspiel

[![Build Status](https://github.com/eeejay/libspeil/workflows/CI/badge.svg)](https://github.com/eeejay/libspeil/actions)

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
