from _common import *


class TestTypes(BaseSpielTest):
    def test_speaker(self):
        def _cb(*args):
            pass

        speechSynthesis = Spiel.Speaker()
        self.assertFalse(speechSynthesis.props.speaking)
        self.assertFalse(speechSynthesis.props.paused)
        speechSynthesis.connect("notify::speaking", _cb)
        speechSynthesis.connect("notify::paused", _cb)
        speechSynthesis.connect("utterance-started", _cb)
        speechSynthesis.connect("range-started", _cb)
        speechSynthesis.connect("utterance-finished", _cb)
        speechSynthesis.connect("utterance-canceled", _cb)

    def test_voice(self):
        voice = Spiel.Voice(
            name="English",
            identifier="en",
            languages=["en", "es", "he"],
            provider_name="org.mock2.Speech.Provider",
        )
        self.assertEqual(voice.props.name, "English")
        self.assertEqual(voice.get_name(), "English")
        self.assertEqual(voice.props.identifier, "en")
        self.assertEqual(voice.get_identifier(), "en")
        self.assertEqual(voice.props.languages, ["en", "es", "he"])
        self.assertEqual(voice.get_languages(), ["en", "es", "he"])
        self.assertEqual(voice.props.provider_name, "org.mock2.Speech.Provider")
        self.assertEqual(voice.get_provider_name(), "org.mock2.Speech.Provider")

    def test_utterance(self):
        utterance = Spiel.Utterance(text="bye")
        self.assertIsNotNone(utterance)
        self.assertEqual(utterance.props.text, "bye")
        self.assertEqual(utterance.get_text(), "bye")
        utterance.set_property("text", "hi")
        self.assertEqual(utterance.props.text, "hi")
        utterance.set_text("yo")
        self.assertEqual(utterance.get_text(), "yo")
        self.assertEqual(utterance.props.volume, 1)
        self.assertEqual(utterance.get_volume(), 1)
        self.assertEqual(utterance.props.rate, 1)
        self.assertEqual(utterance.get_rate(), 1)
        self.assertEqual(utterance.props.pitch, 1)
        self.assertEqual(utterance.get_pitch(), 1)
        utterance.props.volume = 0.5
        self.assertEqual(utterance.props.volume, 0.5)
        self.assertEqual(utterance.get_volume(), 0.5)
        utterance.set_volume(0.334)
        self.assertEqual(utterance.get_property("volume"), 0.334)
        utterance.set_property("pitch", 2)
        self.assertEqual(utterance.get_pitch(), 2)
        utterance.set_pitch(2.2)
        self.assertEqual(utterance.props.pitch, 2.2)
        utterance.set_property("rate", 0.25)
        self.assertEqual(utterance.get_rate(), 0.25)
        utterance.set_rate(0.1)
        self.assertEqual(utterance.props.rate, 0.1)
        self.assertEqual(utterance.props.voice, None)
        self.assertEqual(utterance.get_voice(), None)
        voice = Spiel.Voice(name="English", identifier="en")
        utterance.set_property("voice", voice)
        self.assertEqual(utterance.props.voice.props.name, "English")
        self.assertEqual(utterance.get_voice(), voice)
        self.assertEqual(utterance.props.language, None)
        self.assertEqual(utterance.get_language(), None)
        utterance.set_property("language", "en-gb")
        self.assertEqual(utterance.props.language, "en-gb")
        utterance.set_language("en-us")
        self.assertEqual(utterance.get_language(), "en-us")


if __name__ == "__main__":
    test_main()
