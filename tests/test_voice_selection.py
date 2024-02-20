from _common import *


class TestSpeak(BaseSpielTest):
    def test_lang_settings(self):
        settings = Gio.Settings.new("org.monotonous.libspiel")
        settings["default-voice"] = ("org.mock.Speech.Provider", "ine/hy")
        settings["language-voice-mapping"] = {
            "en": ("org.mock2.Speech.Provider", "gmw/en-GB-x-gbclan")
        }

        utterance = Spiel.Utterance(text="hello world, how are you?", language="en-us")

        speechSynthesis = Spiel.Speaker.new_sync(None)
        self.wait_for_speaking_done(
            speechSynthesis, lambda: speechSynthesis.speak(utterance)
        )
        self.assertEqual(utterance.props.voice.props.name, "English (Lancaster)")

    def test_lang_no_settings(self):
        utterance = Spiel.Utterance(text="hello world, how are you?", language="hy")

        speechSynthesis = Spiel.Speaker.new_sync(None)
        self.wait_for_speaking_done(
            speechSynthesis, lambda: speechSynthesis.speak(utterance)
        )
        self.assertIn("hy", utterance.props.voice.props.languages)

    def test_default_voice(self):
        speechSynthesis = Spiel.Speaker.new_sync(None)
        settings = Gio.Settings.new("org.monotonous.libspiel")
        settings["default-voice"] = ("org.mock.Speech.Provider", "ine/hy")

        utterance = Spiel.Utterance(text="hello world, how are you?", language="hy")

        speechSynthesis = Spiel.Speaker.new_sync(None)
        self.wait_for_speaking_done(
            speechSynthesis, lambda: speechSynthesis.speak(utterance)
        )
        self.assertIn("hy", utterance.props.voice.props.languages)
        self.assertEqual(utterance.props.voice.props.name, "Armenian (East Armenia)")

    def _test_speak_with_voice(self, speechSynthesis, voice):
        utterance = Spiel.Utterance(text="hello world, how are you?", voice=voice)
        speechSynthesis.speak(utterance)
        args = self.mock_iface(
            voice.props.provider.get_well_known_name()
        ).GetLastSpeakArguments()
        self.assertEqual(str(args[2]), voice.props.identifier)

    def test_speak_with_voice_sync(self):
        speechSynthesis = Spiel.Speaker.new_sync(None)
        voice = self.get_voice(speechSynthesis, "org.mock.Speech.Provider", "sit/yue")
        self._test_speak_with_voice(speechSynthesis, voice)

    def test_speak_with_voice_sync_autoexit(self):
        speechSynthesis = Spiel.Speaker.new_sync(None)
        voice = self.get_voice(speechSynthesis, "org.mock3.Speech.Provider", "trk/uz")
        self.wait_for_provider_to_go_away("org.mock3.Speech.Provider")
        self._test_speak_with_voice(speechSynthesis, voice)

    def test_speak_with_voice_async(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        voice = self.get_voice(speechSynthesis, "org.mock.Speech.Provider", "sit/yue")
        self._test_speak_with_voice(speechSynthesis, voice)


if __name__ == "__main__":
    test_main()
