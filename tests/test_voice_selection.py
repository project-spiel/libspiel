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

    def _test_speak_with_voice(self, speechSynthesis, provider_name, voice_id):
        voice = self.get_voice(speechSynthesis, provider_name, voice_id)

        utterance = Spiel.Utterance(text="hello world, how are you?", voice=voice)
        self.wait_for_speaking_changed(
            speechSynthesis, lambda: speechSynthesis.speak(utterance)
        )
        args = self.mock_iface(provider_name).GetLastSpeakArguments()
        self.assertEqual(str(args[2]), voice_id)

    def test_speak_with_voice_sync(self):
        speechSynthesis = Spiel.Speaker.new_sync(None)
        self._test_speak_with_voice(
            speechSynthesis, "org.mock.Speech.Provider", "sit/yue"
        )

    def test_speak_with_voice_sync_autoexit(self):
        speechSynthesis = Spiel.Speaker.new_sync(None)
        self.wait_for_provider_to_go_away("org.mock3.Speech.Provider")
        self._test_speak_with_voice(
            speechSynthesis, "org.mock3.Speech.Provider", "trk/uz"
        )

    def test_speak_with_voice_async(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        self._test_speak_with_voice(
            speechSynthesis, "org.mock.Speech.Provider", "sit/yue"
        )


if __name__ == "__main__":
    test_main()
