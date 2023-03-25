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
            [v.props.name, v.props.identifier, v.props.languages, v.props.provider_name]
            for v in voices
        ]
        voices_info.sort(key=lambda v: v[1])
        _expected_voices = expected_voices[:]
        _expected_voices.sort(key=lambda v: v[1])
        self.assertEqual(
            voices_info,
            _expected_voices,
        )

    def test_add_voice(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        self._test_get_voices(speechSynthesis)
        self.mock_service.AddVoice("Hebrew", "he", ["he", "he-il"])
        self.wait_for_voices_changed(speechSynthesis)
        self._test_get_voices(
            speechSynthesis,
            STANDARD_VOICES + [
                [
                    "Hebrew",
                    "he",
                    ["he", "he-il"],
                    "org.freedesktop.Speech.Synthesis.Mock",
                ]
            ],
        )
        self.mock_service.RemoveVoice("he")
        self.wait_for_voices_changed(speechSynthesis)
        self._test_get_voices(speechSynthesis)

    def test_add_voice_from_inactive(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        self.wait_for_provider_to_go_away("org.freedesktop.Speech.Synthesis.Mock3")
        self.mock_iface("org.freedesktop.Speech.Synthesis.Mock3").AddVoice(
            "Arabic", "ar", ["ar", "ar-ps", "ar-eg"]
        )
        self.wait_for_voices_changed(speechSynthesis)
        self._test_get_voices(
            speechSynthesis,
            STANDARD_VOICES
            + [
                [
                    "Arabic",
                    "ar",
                    ["ar", "ar-ps", "ar-eg"],
                    "org.freedesktop.Speech.Synthesis.Mock3",
                ]
            ],
        )
        self.wait_for_provider_to_go_away("org.freedesktop.Speech.Synthesis.Mock3")
        mock = self.mock_iface("org.freedesktop.Speech.Synthesis.Mock3")
        self.mock_iface("org.freedesktop.Speech.Synthesis.Mock3").RemoveVoice("ar")
        self.wait_for_voices_changed(speechSynthesis)
        self._test_get_voices(speechSynthesis)


if __name__ == "__main__":
    test_main()
