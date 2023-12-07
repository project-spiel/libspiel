from _common import *


class TestInstallProvider(BaseSpielTest):
    def test_install_provider_service(self):
        speechSynthesis = Spiel.Speaker.new_sync(None)

        self.wait_for_provider_to_go_away("org.mock3.Speech.Provider")
        self.uninstall_provider("org.mock3.Speech.Provider")
        self.wait_for_voices_changed(speechSynthesis)
        self.assertEqual(len(speechSynthesis.props.voices), 7)
        self.install_provider("org.mock3.Speech.Provider")
        self.wait_for_voices_changed(speechSynthesis)
        self.assertEqual(len(speechSynthesis.props.voices), 8)


if __name__ == "__main__":
    test_main()
