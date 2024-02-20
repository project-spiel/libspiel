from _common import *


class TestSpeak(BaseSpielTest):
    def test_speak(self):
        speaker = Spiel.Speaker.new_sync(None)

        utterance = Spiel.Utterance(text="hello world, how are you?")
        utterance.props.voice = self.get_voice(
            speaker, "org.mock2.Speech.Provider", "gmw/en-US"
        )

        expected_events = [
            ["notify:speaking", True],
            ["utterance-started", utterance],
            ["word-started", utterance, 0, 6],
            ["sentence-started", utterance, 6, 13],
            ["word-started", utterance, 6, 13],
            ["word-started", utterance, 13, 17],
            ["sentence-started", utterance, 17, 21],
            ["word-started", utterance, 17, 21],
            ["word-started", utterance, 21, 25],
            ["utterance-finished", utterance],
            ["notify:speaking", False],
        ]

        actual_events = self.capture_speak_sequence(speaker, utterance)

        self.assertEqual(actual_events, expected_events)

    def test_queue(self):
        speaker = Spiel.Speaker.new_sync(None)
        [one, two, three] = [
            Spiel.Utterance(text=text) for text in ["one", "two", "three"]
        ]

        expected_events = [
            ["notify:speaking", True],
            ["utterance-started", one],
            ["utterance-finished", one],
            ["utterance-started", two],
            ["utterance-finished", two],
            ["utterance-started", three],
            ["utterance-finished", three],
            ["notify:speaking", False],
        ]

        actual_events = self.capture_speak_sequence(speaker, one, two, three)

        self.assertEqual(actual_events, expected_events)

    def test_pause(self):
        def _started_cb(_speaker, utt):
            _speaker.pause()

        def _notify_paused_cb(_speaker, val):
            if _speaker.props.paused:
                GLib.idle_add(lambda: _speaker.resume())
            else:
                self.mock_service.End()

        self.mock_service.SetInfinite(True)
        speaker = Spiel.Speaker.new_sync(None)
        speaker.connect("utterance-started", _started_cb)
        speaker.connect("notify::paused", _notify_paused_cb)

        utterance = Spiel.Utterance(text="hello world, how are you?")

        expected_events = [
            ["notify:speaking", True],
            ["utterance-started", utterance],
            ["notify:paused", True],
            ["notify:paused", False],
            ["utterance-finished", utterance],
            ["notify:speaking", False],
        ]

        actual_events = self.capture_speak_sequence(speaker, utterance)

        self.assertEqual(actual_events, expected_events)

    def test_cancel(self):
        def _started_cb(_speaker, utt):
            GLib.idle_add(lambda: _speaker.cancel())

        self.mock_service.SetInfinite(True)

        speaker = Spiel.Speaker.new_sync(None)
        speaker.connect("utterance-started", _started_cb)

        [one, two, three] = [
            Spiel.Utterance(text=text) for text in ["one", "two", "three"]
        ]

        expected_events = [
            ["notify:speaking", True],
            ["utterance-started", one],
            ["utterance-canceled", one],
            ["notify:speaking", False],
        ]
        actual_events = self.capture_speak_sequence(speaker, one, two, three)

        self.assertEqual(actual_events, expected_events)

    def test_pause_and_cancel(self):
        def _started_cb(_speaker, utt):
            GLib.idle_add(lambda: _speaker.pause())

        def _notify_paused_cb(_speaker, val):
            GLib.idle_add(lambda: _speaker.cancel())

        actual_events = []

        self.mock_service.SetInfinite(True)
        speaker = Spiel.Speaker.new_sync(None)
        speaker.connect("utterance-started", _started_cb)
        speaker.connect("notify::paused", _notify_paused_cb)

        utterance = Spiel.Utterance(text="hello world, how are you?")

        expected_events = [
            ["notify:speaking", True],
            ["utterance-started", utterance],
            ["notify:paused", True],
            ["utterance-canceled", utterance],
            ["notify:speaking", False],
        ]

        actual_events = self.capture_speak_sequence(speaker, utterance)

        self.assertEqual(actual_events, expected_events)

    def test_pause_then_speak(self):
        def _notify_paused_cb(_speaker, val):
            if _speaker.props.paused:
                GLib.idle_add(lambda: _speaker.resume())

        speaker = Spiel.Speaker.new_sync(None)
        speaker.connect("notify::paused", _notify_paused_cb)
        GLib.idle_add(lambda: speaker.pause())

        utterance = Spiel.Utterance(text="hello world, how are you?")

        expected_events = [
            ["notify:paused", True],
            ["notify:speaking", True],
            ["notify:paused", False],
            ["utterance-started", utterance],
            ["utterance-finished", utterance],
            ["notify:speaking", False],
        ]

        actual_events = self.capture_speak_sequence(speaker, utterance)

        self.assertEqual(actual_events, expected_events)

    def test_is_ssml(self):
        speaker = Spiel.Speaker.new_sync(None)

        utterance = Spiel.Utterance(text="hello world, how are you?", language="hy")
        self.wait_for_speaking_done(speaker, lambda: speaker.speak(utterance))
        is_ssml = self.mock_service.GetLastSpeakArguments()[-1]
        self.assertFalse(is_ssml)

        utterance = Spiel.Utterance(
            text="hello world, how are you?", language="hy", is_ssml=True
        )
        self.wait_for_speaking_done(speaker, lambda: speaker.speak(utterance))
        is_ssml = self.mock_service.GetLastSpeakArguments()[-1]
        self.assertTrue(is_ssml)


if __name__ == "__main__":
    test_main()
