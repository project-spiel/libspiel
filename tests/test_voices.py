from _common import *


class TestVoices(BaseSpielTest):
    def test_voice_features(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        voice = self.get_voice(speechSynthesis, "org.one.Speech.Provider", "sit/yue")
        self.assertTrue(voice.get_features() & Spiel.VoiceFeature.SSML_SAY_AS_CARDINAL)
        voice = self.get_voice(speechSynthesis, "org.one.Speech.Provider", "ine/hy")
        self.assertEqual(voice.get_features(), 0)

    def test_get_async_voices(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        self._do_test_voices_prop(speechSynthesis)
        del speechSynthesis

        # A new sync speaker should have 0 voices since the registry was garbage collected
        speechSynthesis = Spiel.Speaker.new_sync(None)
        self.assertEqual(len(speechSynthesis.props.voices), 0)

        self.wait_for_voices_changed(speechSynthesis, count=8)
        self._do_test_voices_prop(speechSynthesis)

    def _do_test_voices_prop(self, speechSynthesis, expected_voices=STANDARD_VOICES):
        voices = speechSynthesis.props.voices
        voices_info = [
            [
                v.props.provider.props.identifier,
                v.props.name,
                v.props.identifier,
                v.props.languages,
            ]
            for v in voices
        ]
        _expected_voices = expected_voices[:]
        _expected_voices.sort(key=lambda v: "-".join(v[:3]))
        self.assertEqual(
            voices_info,
            _expected_voices,
        )

    def test_add_voice(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        mock = self.mock_iface("org.one.Speech.Provider")
        self._do_test_voices_prop(speechSynthesis)

        mock.AddVoice(
            "Hebrew",
            "he",
            "audio/x-raw,format=S16LE,channels=1,rate=22050",
            0,
            ["he", "he-il"],
        )
        self.wait_for_voices_changed(speechSynthesis, added=["he"])
        self._do_test_voices_prop(
            speechSynthesis,
            STANDARD_VOICES
            + [
                [
                    "org.one.Speech.Provider",
                    "Hebrew",
                    "he",
                    ["he", "he-il"],
                ]
            ],
        )
        mock.RemoveVoice("he")
        self.wait_for_voices_changed(speechSynthesis, removed=["he"])
        self._do_test_voices_prop(speechSynthesis)

    def test_add_voice_from_inactive(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        self.kill_provider("org.three.Speech.Provider")
        self.wait_for_provider_to_go_away("org.three.Speech.Provider")
        self.mock_iface("org.three.Speech.Provider").AddVoice(
            "Arabic",
            "ar",
            "audio/x-raw,format=S16LE,channels=1,rate=22050",
            0,
            ["ar", "ar-ps", "ar-eg"],
        )

        self.wait_for_voices_changed(speechSynthesis, added=["ar"])
        self._do_test_voices_prop(
            speechSynthesis,
            STANDARD_VOICES
            + [
                [
                    "org.three.Speech.Provider",
                    "Arabic",
                    "ar",
                    ["ar", "ar-ps", "ar-eg"],
                ]
            ],
        )

        self.kill_provider("org.three.Speech.Provider")
        self.wait_for_provider_to_go_away("org.three.Speech.Provider")
        self.mock_iface("org.three.Speech.Provider").RemoveVoice("ar")
        self.wait_for_voices_changed(speechSynthesis, removed=["ar"])
        self._do_test_voices_prop(speechSynthesis)


if __name__ == "__main__":
    test_main()
