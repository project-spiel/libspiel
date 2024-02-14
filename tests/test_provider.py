from _common import *


class TestInstallProvider(BaseSpielTest):
    def test_providers_speaker_property(self):
        speaker = self.wait_for_async_speaker_init()
        self.assertEqual(
            [p.get_well_known_name() for p in speaker.props.providers],
            [
                "org.mock.Speech.Provider",
                "org.mock2.Speech.Provider",
                "org.mock3.Speech.Provider",
            ],
        )
        self.assertEqual(
            [len(p.get_voices()) for p in speaker.get_providers()],
            [2, 5, 1],
        )

    def test_install_provider_service(self):
        speaker = Spiel.Speaker.new_sync(None)

        self.wait_for_provider_to_go_away("org.mock3.Speech.Provider")
        self.uninstall_provider("org.mock3.Speech.Provider")
        self.wait_for_voices_changed(speaker, removed=["trk/uz"])
        self.assertEqual(len(speaker.props.voices), 7)
        self.install_provider("org.mock3.Speech.Provider")
        self.wait_for_voices_changed(speaker, added=["trk/uz"])
        self.assertEqual(len(speaker.props.voices), 8)


if __name__ == "__main__":
    test_main()
