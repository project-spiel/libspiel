from _common import *


class TestInstallProvider(BaseSpielTest):
    def _test_portal(self):
        session_bus = dbus.SessionBus()
        proxy = session_bus.get_object(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
        )
        portal = dbus.Interface(
            proxy, dbus_interface="org.freedesktop.DBus.Introspectable"
        )
        self.assertIn("org.freedesktop.portal.Speech", portal.Introspect())

    def test_providers_speaker_property(self):
        speaker = self.wait_for_async_speaker_init()
        self.assertEqual(
            [p.get_identifier() for p in speaker.props.providers],
            [
                "org.one.Speech.Provider",
                "org.three.Speech.Provider",
                "org.two.Speech.Provider",
            ],
        )
        self.assertEqual(
            [len(p.get_voices()) for p in speaker.get_providers()],
            [2, 1, 5],
        )
        self.assertEqual(
            [p.props.name for p in speaker.get_providers()],
            [
                "Speech Provider (one)",
                "Speech Provider (three)",
                "Speech Provider (two)",
            ],
        )

    def test_install_provider_service(self):
        speaker = self.wait_for_async_speaker_init()

        self.kill_provider("org.three.Speech.Provider")
        self.wait_for_provider_to_go_away("org.three.Speech.Provider")

        self.uninstall_providers(["org.three.Speech.Provider"])
        self.wait_for_voices_changed(speaker, removed=["trk/uz"])
        self.assertEqual(len(speaker.props.voices), 7)

        self.install_providers(["org.three.Speech.Provider"])
        self.wait_for_voices_changed(speaker, added=["trk/uz"])
        self.assertEqual(len(speaker.props.voices), 8)


if __name__ == "__main__":
    test_main()
