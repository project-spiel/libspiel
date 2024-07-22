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

TEMPLATE_DIR = os.path.dirname(os.path.realpath(__file__))


class ProviderTestFixture(dbusmock.DBusTestCase):
    """A test fixture for Spiel."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.start_session_bus()
        cls.dbus_con = cls.get_dbus(system_bus=False)

    def setUp(self) -> None:

        (self.p_mock, self.obj_speechprovider) = self.spawn_server_template(
            f"{TEMPLATE_DIR}/speechprovider.py", {}, stdout=subprocess.PIPE
        )

        # Set output to non-blocking
        flags = fcntl.fcntl(self.p_mock.stdout, fcntl.F_GETFL)
        fcntl.fcntl(self.p_mock.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def tearDown(self) -> None:
        self.p_mock.stdout.close()
        self.p_mock.terminate()
        self.p_mock.wait()

    def test_run(self) -> None:
        subprocess.run(
            [os.environ.get("G_TEST_EXE", ""), "--tap"],
            check=True,
            encoding="utf-8",
            stderr=sys.stderr,
            stdout=sys.stdout,
        )


if __name__ == "__main__":
    # Output to stderr; we're forwarding TAP output of the real program
    runner = unittest.TextTestRunner(stream=sys.stderr, verbosity=2)
    unittest.main(testRunner=runner)
