from _common import *


class TestVoices(BaseSpielTest):
    def test_get_async_voices(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        self._test_get_voices(speechSynthesis)

    def test_get_sync_voices(self):
        speechSynthesis = Spiel.Speaker.new_sync(None)
        self._test_get_voices(speechSynthesis)

    def _test_get_voices(self, speechSynthesis, expected_voices=STANDARD_VOICES):
        voices = speechSynthesis.props.voices
        voices_info = [
            [v.props.provider_name, v.props.name, v.props.identifier, v.props.languages]
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
        self._test_get_voices(speechSynthesis)
        self.mock_service.AddVoice("Hebrew", "he", ["he", "he-il"])
        self.wait_for_voices_changed(speechSynthesis, added=["he"])
        self._test_get_voices(
            speechSynthesis,
            STANDARD_VOICES
            + [
                [
                    "org.mock.Speech.Provider",
                    "Hebrew",
                    "he",
                    ["he", "he-il"],
                ]
            ],
        )
        self.mock_service.RemoveVoice("he")
        self.wait_for_voices_changed(speechSynthesis, removed=["he"])
        self._test_get_voices(speechSynthesis)

    def test_add_voice_from_inactive(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        self.wait_for_provider_to_go_away("org.mock3.Speech.Provider")
        self.mock_iface("org.mock3.Speech.Provider").AddVoice(
            "Arabic", "ar", ["ar", "ar-ps", "ar-eg"]
        )
        self.wait_for_voices_changed(speechSynthesis, added=["ar"])
        self._test_get_voices(
            speechSynthesis,
            STANDARD_VOICES
            + [
                [
                    "org.mock3.Speech.Provider",
                    "Arabic",
                    "ar",
                    ["ar", "ar-ps", "ar-eg"],
                ]
            ],
        )
        self.wait_for_provider_to_go_away("org.mock3.Speech.Provider")
        self.mock_iface("org.mock3.Speech.Provider").RemoveVoice("ar")
        self.wait_for_voices_changed(speechSynthesis, removed=["ar"])
        self._test_get_voices(speechSynthesis)


if __name__ == "__main__":
    test_main()
