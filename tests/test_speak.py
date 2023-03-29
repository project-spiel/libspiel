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

        def _reached_word_cb(synth, utt):
            actual_events.append("word-reached")
            self.assertEqual(utt, utterance)

        def _finished_cb(synth, utt):
            actual_events.append("utterance-finished")
            self.assertEqual(utt, utterance)

        expected_events = [
            "notify:speaking=True",
            "utterance-started",
            "word-reached",
            "word-reached",
            "word-reached",
            "word-reached",
            "word-reached",
            "utterance-finished",
            "notify:speaking=False",
        ]

        actual_events = []

        speechSynthesis = Spiel.Speaker.new_sync(None)
        self.assertFalse(speechSynthesis.props.speaking)
        self.assertFalse(speechSynthesis.props.paused)
        speechSynthesis.connect("notify::speaking", _notify_speaking_cb)
        speechSynthesis.connect("utterance-started", _started_cb)
        speechSynthesis.connect("word-reached", _reached_word_cb)
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
                self.mock_service.SetAutoStep(True)

        def _finished_cb(synth, utt):
            actual_events.append("utterance-finished")
            self.assertFalse(speechSynthesis.props.paused)
            self.assertEqual(
                actual_events,
                ["utterance-started", "notify:paused=True", "notify:paused=False", "utterance-finished"],
            )
            loop.quit()

        actual_events = []

        self.mock_service.SetAutoStep(False)
        speechSynthesis = Spiel.Speaker.new_sync(None)
        speechSynthesis.connect("utterance-started", _started_cb)
        speechSynthesis.connect("notify::paused", _notify_paused_cb)
        speechSynthesis.connect("utterance-finished", _finished_cb)
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

        self.mock_service.SetAutoStep(False)
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

        self.mock_service.SetAutoStep(False)
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

    def _test_speak_with_voice(self, speechSynthesis, provider_name, voice_id):
        voice = None
        for v in speechSynthesis.props.voices:
            if (
                v.props.provider_name == provider_name
                and v.props.identifier == voice_id
            ):
                voice = v

        utterance = Spiel.Utterance(text="hello world, how are you?", voice=voice)
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
            speechSynthesis, "org.mock3.Speech.Provider", "gmw/en"
        )

    def test_speak_with_voice_async(self):
        speechSynthesis = self.wait_for_async_speaker_init()
        self._test_speak_with_voice(
            speechSynthesis, "org.mock.Speech.Provider", "sit/yue"
        )


if __name__ == "__main__":
    test_main()
