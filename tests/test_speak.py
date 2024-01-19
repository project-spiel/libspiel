from _common import *


class TestSpeak(BaseSpielTest):
    def test_speak(self):
        def _notify_speaking_cb(synth, val):
            actual_events.append("notify:speaking=%s" % synth.props.speaking)
            if not synth.props.speaking:
                self.assertEqual(actual_events, expected_events)
                loop.quit()

        def _started_cb(synth, utt):
            actual_events.append("utterance-started")
            self.assertEqual(utt, utterance)

        expected_word_offsets = [[0, 6], [6, 13], [13, 17], [17, 21], [21, 25]]

        def _range_started_cb(synth, utt, start, end):
            expected_start, expected_end = expected_word_offsets.pop(0)
            self.assertEqual(expected_end, end)
            self.assertEqual(expected_start, start)
            actual_events.append("range-started")
            self.assertEqual(utt, utterance)

        def _finished_cb(synth, utt):
            actual_events.append("utterance-finished")
            self.assertEqual(utt, utterance)

        expected_events = [
            "notify:speaking=True",
            "utterance-started",
            # "range-started",
            # "range-started",
            # "range-started",
            # "range-started",
            # "range-started",
            "utterance-finished",
            "notify:speaking=False",
        ]

        actual_events = []

        speechSynthesis = Spiel.Speaker.new_sync(None)
        self.assertFalse(speechSynthesis.props.speaking)
        self.assertFalse(speechSynthesis.props.paused)
        speechSynthesis.connect("notify::speaking", _notify_speaking_cb)
        speechSynthesis.connect("utterance-started", _started_cb)
        speechSynthesis.connect("range-started", _range_started_cb)
        speechSynthesis.connect("utterance-finished", _finished_cb)
        utterance = Spiel.Utterance(text="hello world, how are you?")
        speechSynthesis.speak(utterance)

        loop = GLib.MainLoop()
        loop.run()

    def test_queue(self):
        actual_events = []

        def _notify_speaking_cb(synth, val):
            actual_events.append("notify:speaking=%s" % synth.props.speaking)
            if not speechSynthesis.props.speaking:
                self.assertEqual(
                    actual_events,
                    [
                        "notify:speaking=True",
                        "started 'one'",
                        "finished 'one'",
                        "started 'two'",
                        "finished 'two'",
                        "started 'three'",
                        "finished 'three'",
                        "notify:speaking=False",
                    ],
                )
                loop.quit()

        def _started_cb(synth, utt):
            actual_events.append("started '%s'" % utt.props.text)
            self.assertTrue(speechSynthesis.props.speaking)

        def _finished_cb(synth, utt):
            actual_events.append("finished '%s'" % utt.props.text)

        speechSynthesis = Spiel.Speaker.new_sync(None)
        speechSynthesis.connect("notify::speaking", _notify_speaking_cb)
        speechSynthesis.connect("utterance-started", _started_cb)
        speechSynthesis.connect("utterance-finished", _finished_cb)

        for text in ["one", "two", "three"]:
            utterance = Spiel.Utterance(text=text)
            speechSynthesis.speak(utterance)
        loop = GLib.MainLoop()
        loop.run()

    def test_pause(self):
        def _started_cb(synth, utt):
            actual_events.append("utterance-started")
            self.assertFalse(speechSynthesis.props.paused)
            synth.pause()

        def _notify_paused_cb(synth, val):
            actual_events.append("notify:paused=%s" % synth.props.paused)
            if speechSynthesis.props.paused:
                synth.resume()
            else:
                synth.cancel()

        def _canceled_cb(synth, utt):
            actual_events.append("utterance-canceled")
            self.assertFalse(speechSynthesis.props.paused)
            self.assertEqual(
                actual_events,
                [
                    "utterance-started",
                    "notify:paused=True",
                    "notify:paused=False",
                    "utterance-canceled",
                ],
            )
            loop.quit()

        actual_events = []

        self.mock_service.SetInfinite(True)
        speechSynthesis = Spiel.Speaker.new_sync(None)
        speechSynthesis.connect("utterance-started", _started_cb)
        speechSynthesis.connect("notify::paused", _notify_paused_cb)
        speechSynthesis.connect("utterance-canceled", _canceled_cb)
        utterance = Spiel.Utterance(text="hello world, how are you?")
        speechSynthesis.speak(utterance)

        loop = GLib.MainLoop()
        loop.run()

    def test_cancel(self):
        actual_events = []

        def _notify_speaking_cb(synth, val):
            actual_events.append("notify:speaking=%s" % synth.props.speaking)
            if not speechSynthesis.props.speaking:
                self.assertEqual(
                    actual_events,
                    [
                        "notify:speaking=True",
                        "started 'one'",
                        "canceled 'one'",
                        "notify:speaking=False",
                    ],
                )
                loop.quit()

        def _started_cb(synth, utt):
            actual_events.append("started '%s'" % utt.props.text)
            self.assertTrue(speechSynthesis.props.speaking)
            speechSynthesis.cancel()

        def _canceled_cb(synth, utt):
            actual_events.append("canceled '%s'" % utt.props.text)

        self.mock_service.SetInfinite(True)
        speechSynthesis = Spiel.Speaker.new_sync(None)
        speechSynthesis.connect("notify::speaking", _notify_speaking_cb)
        speechSynthesis.connect("utterance-started", _started_cb)
        speechSynthesis.connect("utterance-canceled", _canceled_cb)

        for text in ["one", "two", "three"]:
            utterance = Spiel.Utterance(text=text)
            speechSynthesis.speak(utterance)
        loop = GLib.MainLoop()
        loop.run()

    def test_pause_and_cancel(self):
        def _notify_speaking_cb(synth, val):
            actual_events.append("notify:speaking=%s" % synth.props.speaking)
            if not speechSynthesis.props.speaking:
                self.assertEqual(
                    actual_events,
                    [
                        "notify:speaking=True",
                        "utterance-started",
                        "notify:paused=True",
                        "utterance-canceled",
                        "notify:speaking=False",
                    ],
                )
                loop.quit()

        def _started_cb(synth, utt):
            actual_events.append("utterance-started")
            self.assertFalse(speechSynthesis.props.paused)
            synth.pause()

        def _notify_paused_cb(synth, val):
            actual_events.append("notify:paused=%s" % synth.props.speaking)
            synth.cancel()

        def _canceled_cb(synth, utt):
            actual_events.append("utterance-canceled")
            self.assertTrue(speechSynthesis.props.paused)
            loop.quit()

        actual_events = []

        self.mock_service.SetInfinite(True)
        speechSynthesis = Spiel.Speaker.new_sync(None)
        speechSynthesis.connect("utterance-started", _started_cb)
        speechSynthesis.connect("notify::speaking", _notify_speaking_cb)
        speechSynthesis.connect("notify::paused", _notify_paused_cb)
        speechSynthesis.connect("utterance-canceled", _canceled_cb)
        utterance = Spiel.Utterance(text="hello world, how are you?")
        speechSynthesis.speak(utterance)

        loop = GLib.MainLoop()
        loop.run()

    def test_pause_then_speak(self):
        def _notify_speaking_cb(synth, val):
            actual_events.append("notify:speaking=%s" % synth.props.speaking)
            if not synth.props.speaking:
                self.assertEqual(
                    actual_events,
                    [
                        "notify:paused=True",
                        "notify:paused=False",
                        "notify:speaking=True",
                        "utterance-started",
                        "utterance-finished",
                        "notify:speaking=False",
                    ],
                )
                loop.quit()

        def _notify_paused_cb(synth, val):
            actual_events.append("notify:paused=%s" % synth.props.paused)
            if synth.props.paused:
                synth.resume()

        def _started_cb(synth, utt):
            actual_events.append("utterance-started")
            self.assertEqual(utt, utterance)
            self.assertTrue(speechSynthesis.props.speaking)

        def _finished_cb(synth, utt):
            actual_events.append("utterance-finished")
            self.assertEqual(utt, utterance)

        actual_events = []

        speechSynthesis = Spiel.Speaker.new_sync(None)
        speechSynthesis.connect("notify::speaking", _notify_speaking_cb)
        speechSynthesis.connect("notify::paused", _notify_paused_cb)
        speechSynthesis.connect("utterance-started", _started_cb)
        speechSynthesis.connect("utterance-finished", _finished_cb)
        speechSynthesis.pause()
        utterance = Spiel.Utterance(text="hello world, how are you?")
        speechSynthesis.speak(utterance)

        loop = GLib.MainLoop()
        loop.run()


if __name__ == "__main__":
    test_main()
