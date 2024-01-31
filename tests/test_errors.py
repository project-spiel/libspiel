from _common import *


class TestSpeak(BaseSpielTest):
    def test_provider_dies(self):
        def _notify_speaking_cb(synth, val):
            actual_events.append("notify:speaking=%s" % synth.props.speaking)
            if not synth.props.speaking:
                self.assertEqual(actual_events, expected_events)
                loop.quit()

        def _started_cb(synth, utt):
            actual_events.append("utterance-started")
            self.assertEqual(utt, utterance)
            self.mock_service.Die()

        def _error_cb(synth, utt, err):
            self.assertTrue(
                err.matches(Spiel.error_quark(), Spiel.Error.PROVIDER_UNEXPECTEDLY_DIED)
            )
            actual_events.append("utterance-error")
            self.assertEqual(utt, utterance)

        expected_events = [
            "notify:speaking=True",
            "utterance-started",
            "utterance-error",
            "notify:speaking=False",
        ]

        actual_events = []

        self.mock_service.SetInfinite(True)
        speechSynthesis = Spiel.Speaker.new_sync(None)
        self.assertFalse(speechSynthesis.props.speaking)
        self.assertFalse(speechSynthesis.props.paused)
        speechSynthesis.connect("notify::speaking", _notify_speaking_cb)
        speechSynthesis.connect("utterance-started", _started_cb)
        speechSynthesis.connect("utterance-error", _error_cb)
        utterance = Spiel.Utterance(text="hello world, how are you?")
        speechSynthesis.speak(utterance)

        loop = GLib.MainLoop()
        loop.run()

    def test_bad_voice(self):
        def _notify_speaking_cb(synth, val):
            actual_events.append("notify:speaking=%s" % synth.props.speaking)
            if not synth.props.speaking:
                self.assertEqual(actual_events, expected_events)
                loop.quit()

        def _started_cb(synth, utt):
            actual_events.append("utterance-started")

        def _finished_cb(synth, utt):
            actual_events.append("utterance-finished")

        def _error_cb(synth, utt, err):
            self.assertTrue(
                err.matches(Spiel.error_quark(), Spiel.Error.MISCONFIGURED_VOICE)
            )
            actual_events.append("utterance-error")

        expected_events = [
            "notify:speaking=True",
            "utterance-started",
            "utterance-finished",
            "utterance-error",
            "utterance-started",
            "utterance-finished",
            "notify:speaking=False",
        ]

        actual_events = []

        self.mock_service.SetInfinite(True)
        speechSynthesis = Spiel.Speaker.new_sync(None)
        self.assertFalse(speechSynthesis.props.speaking)
        self.assertFalse(speechSynthesis.props.paused)
        speechSynthesis.connect("notify::speaking", _notify_speaking_cb)
        speechSynthesis.connect("utterance-started", _started_cb)
        speechSynthesis.connect("utterance-error", _error_cb)
        speechSynthesis.connect("utterance-finished", _finished_cb)

        for provider_name, voice_id in [
            ("org.mock2.Speech.Provider", "ine/hyw"),
            ("org.mock2.Speech.Provider", "gmw/en-GB-scotland"),
            ("org.mock2.Speech.Provider", "gmw/en-GB-x-gbclan"),
        ]:
            utterance = Spiel.Utterance(text="hello world, how are you?")
            utterance.props.voice = self.get_voice(
                speechSynthesis, provider_name, voice_id
            )
            speechSynthesis.speak(utterance)

        loop = GLib.MainLoop()
        loop.run()


if __name__ == "__main__":
    test_main()
