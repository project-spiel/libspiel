from _common import *


class TestSpeak(BaseSpielTest):
    def _test_speak_with_voice(self, speechSynthesis, provider_name, voice_id):
        voice = None
        for v in speechSynthesis.props.voices:
            if (
                v.props.provider_name == provider_name
                and v.props.identifier == voice_id
            ):
                voice = v

        utterance = Spiel.Utterance(text="hello world, how are you?", voice=voice)
        self.mock_iface(provider_name).SetAutoStep(False)
        self.wait_for_speaking_changed(
            speechSynthesis, lambda: speechSynthesis.speak(utterance)
        )
        args = self.mock_iface(provider_name).GetLastSpeakArguments()
        self.assertEqual(str(args[2]), voice_id)
        self.wait_for_speaking_changed(
            speechSynthesis, lambda: self.mock_iface(provider_name).SetAutoStep(True)
        )

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
