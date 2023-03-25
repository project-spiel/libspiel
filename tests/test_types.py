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
        speechSynthesis.connect("started", _cb)
        speechSynthesis.connect("word-reached", _cb)
        speechSynthesis.connect("finished", _cb)
        speechSynthesis.connect("canceled", _cb)

    def test_voice(self):
        voice = Spiel.Voice(
            name="English",
            identifier="en",
            languages=["en", "es", "he"],
            provider_name="org.freedesktop.Speech.Synthesis.Mock2",
        )
        self.assertEqual(voice.props.name, "English")
        self.assertEqual(voice.props.identifier, "en")
        self.assertEqual(voice.props.languages, ["en", "es", "he"])
        self.assertEqual(
            voice.props.provider_name, "org.freedesktop.Speech.Synthesis.Mock2"
        )

    def test_utterance(self):
        utterance = Spiel.Utterance(text="bye")
        self.assertIsNotNone(utterance)
        self.assertEqual(utterance.props.text, "bye")
        utterance.set_property("text", "hi")
        self.assertEqual(utterance.props.text, "hi")
        self.assertEqual(utterance.props.volume, 1)
        self.assertEqual(utterance.props.rate, 1)
        self.assertEqual(utterance.props.pitch, 1)
        utterance.props.volume = 0.5
        self.assertEqual(utterance.props.volume, 0.5)
        self.assertEqual(utterance.get_property("volume"), 0.5)
        utterance.set_property("pitch", 2)
        self.assertEqual(utterance.props.pitch, 2)
        utterance.set_property("rate", 0.25)
        self.assertEqual(utterance.props.rate, 0.25)
        self.assertEqual(utterance.props.voice, None)
        voice = Spiel.Voice(name="English", identifier="en")
        utterance.set_property("voice", voice)
        self.assertEqual(utterance.props.voice.props.name, "English")


if __name__ == "__main__":
    test_main()
