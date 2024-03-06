#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright © 2024 GNOME Foundation Inc.
# SPDX-FileContributor: Andy Holmes <andyholmes@gnome.org>


"""This module provides a test fixture for Spiel."""


import fcntl
import os
import subprocess
import sys
import unittest

import dbusmock
import dbus
from dbusmock import MOCK_IFACE

PROVIDER_NAME = 'org.mock.Speech.Provider'
PROVIDER_PATH = '/org/mock/Speech/Provider'
PROVIDER_IFACE = 'org.freedesktop.Speech.Provider'

@dbus.service.method(MOCK_IFACE, in_signature='hssddbs', out_signature='')
def Synthesize(self, pipe_fd, text, voice_id, pitch, rate, is_ssml, language):
    '''When called, the speech provider will send the synthesized output to the
    given file descriptor.
    '''
    def write_audio(fd, _cond):
        target = io.FileIO(fd, 'wb', closefd=True)
        # TODO: write mock audio?
        return GLib.SOURCE_REMOVE

    GLib.unix_fd_add_full(GLib.PRIORITY_HIGH,
                          pipe_fd,
                          GLib.IO_OUT,
                          write_audio)

class ProviderTestFixture(dbusmock.DBusTestCase):
    """A test fixture for Spiel."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.start_session_bus()
        cls.dbus_con = cls.get_dbus(system_bus=False)

    def setUp(self) -> None:
        self.p_mock = self.spawn_server(PROVIDER_NAME,
                                        PROVIDER_PATH,
                                        PROVIDER_IFACE,
                                        system_bus=False,
                                        stdout=subprocess.PIPE)

        # Get a proxy for the provider object's Mock interface
        mock = dbus.Interface(self.dbus_con.get_object(PROVIDER_NAME,
                                                       PROVIDER_PATH),
                              dbusmock.MOCK_IFACE)

        mock.AddProperties(PROVIDER_IFACE, {
            'Name': 'Mock Provider',
            'Voices': dbus.Array([
                 ('Afrikaans',         'gmw/af',    'audio/x-spiel,format=S16LE,channels=1,rate=22050', dbus.UInt64(242123), ['af']),
                 ('Cherokee ',         'iro/chr',   'audio/x-spiel,format=S16LE,channels=1,rate=22050', dbus.UInt64(242123), ['chr-US-Qaaa-x-west']),
                 ('English (America)', 'gmw/en-US', 'audio/x-spiel,format=S16LE,channels=1,rate=22050', dbus.UInt64(242123), ['en-us', 'en']),
            ], signature='(ssstas)')
        })

        # Set output to non-blocking
        flags = fcntl.fcntl(self.p_mock.stdout, fcntl.F_GETFL)
        fcntl.fcntl(self.p_mock.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def tearDown(self) -> None:
        self.p_mock.stdout.close()
        self.p_mock.terminate()
        self.p_mock.wait()

    def test_run(self) -> None:
        subprocess.run([os.environ.get('G_TEST_EXE', ''), '--tap'],
                       check=True,
                       encoding='utf-8',
                       stderr=sys.stderr,
                       stdout=sys.stdout)


if __name__ == '__main__':
    # Output to stderr; we're forwarding TAP output of the real program
    runner = unittest.TextTestRunner(stream=sys.stderr, verbosity=2)
    unittest.main(testRunner=runner)
