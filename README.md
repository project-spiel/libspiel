# libspiel

[![Documentation](https://github.com/eeejay/libspiel/actions/workflows/documentation.yml/badge.svg)](https://github.com/eeejay/libspiel/actions)
[![Tests](https://github.com/eeejay/libspiel/actions/workflows/tests.yml/badge.svg)](https://github.com/eeejay/libspiel/actions)

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
