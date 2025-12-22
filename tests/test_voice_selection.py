from _common import *


class TestSpeak(BaseSpielTest):
    def test_lang_settings(self):
        settings = Gio.Settings.new("org.monotonous.libspiel")
        settings["default-voice"] = ("org.one.Speech.Provider", "ine/hy")
        settings["language-voice-mapping"] = {
            "en": ("org.two.Speech.Provider", "gmw/en-GB-x-gbclan")
        }

        utterance = Spiel.Utterance(text="hello world, how are you?", language="en-us")

        speechSynthesis = self.wait_for_async_speaker_init()
        self.wait_for_speaking_done(
            speechSynthesis, lambda: speechSynthesis.speak(utterance)
        )
        self.assertEqual(utterance.props.voice.props.name, "English (Lancaster)")

    def test_lang_no_settings(self):
        utterance = Spiel.Utterance(text="hello world, how are you?", language="hy")

        speechSynthesis = self.wait_for_async_speaker_init()
        self.wait_for_speaking_done(
            speechSynthesis, lambda: speechSynthesis.speak(utterance)
        )
        self.assertIn("hy", utterance.props.voice.props.languages)

    def test_default_voice(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        settings = Gio.Settings.new("org.monotonous.libspiel")
        settings["default-voice"] = ("org.one.Speech.Provider", "sit/yue")

        utterance = Spiel.Utterance(text="hello world, how are you?", language="zh")

        speechSynthesis = self.wait_for_async_speaker_init()
        self.wait_for_speaking_done(
            speechSynthesis, lambda: speechSynthesis.speak(utterance)
        )
        self.assertIn("zh", utterance.props.voice.props.languages)
        self.assertEqual(utterance.props.voice.props.name, "Chinese (Cantonese)")

    def _do_test_speak_with_voice(self, speechSynthesis, voice):
        utterance = Spiel.Utterance(text="hello world, how are you?", voice=voice)
        speechSynthesis.speak(utterance)
        args = self.mock_iface(
            voice.props.provider.get_identifier()
        ).GetLastSpeakArguments()
        self.assertEqual(str(args[2]), voice.props.identifier)

    def test_speak_with_voice_sync(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        voice = self.get_voice(speechSynthesis, "org.one.Speech.Provider", "sit/yue")
        self._do_test_speak_with_voice(speechSynthesis, voice)

    def test_speak_with_voice_sync_autoexit(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        voice = self.get_voice(speechSynthesis, "org.three.Speech.Provider", "trk/uz")
        self.wait_for_provider_to_go_away("org.mock3.Speech.Provider")
        self._do_test_speak_with_voice(speechSynthesis, voice)

    def test_speak_with_voice_async(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        voice = self.get_voice(speechSynthesis, "org.one.Speech.Provider", "sit/yue")
        self._do_test_speak_with_voice(speechSynthesis, voice)


if __name__ == "__main__":
    test_main()
