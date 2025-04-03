from _common import *


class TestSpeak(BaseSpielTest):
    def test_provider_dies(self):
        speaker = Spiel.Speaker.new_sync(None)

        utterance = Spiel.Utterance(text="die")
        # XXX: fdsrc goes to playing state even when no
        # data is written to the pipe. So we use a spielsrc.
        utterance.props.voice = self.get_voice(
            speaker, "org.mock2.Speech.Provider", "gmw/en-US"
        )

        expected_error = (
            "g-dbus-error-quark",
            int(Gio.DBusError.NO_REPLY),
            "GDBus.Error:org.freedesktop.DBus.Error.NoReply: "
            "Message recipient disconnected from message bus without replying",
        )
        expected_events = [
            ["notify:speaking", True],
            ["utterance-error", utterance, expected_error],
            ["notify:speaking", False],
        ]

        actual_events = self.capture_speak_sequence(speaker, utterance)

        self.assertEqual(actual_events, expected_events)

    def test_bad_voice(self):
        self.mock_service.SetInfinite(True)
        speaker = Spiel.Speaker.new_sync(None)

        voices = [
            self.get_voice(speaker, *provider_identifier_and_id)
            for provider_identifier_and_id in [
                ("org.mock2.Speech.Provider", "ine/hyw"),
                ("org.mock2.Speech.Provider", "gmw/en-GB-scotland#misconfigured"),
                ("org.mock2.Speech.Provider", "gmw/en-GB-x-gbclan"),
            ]
        ]

        [one, two, three] = [
            Spiel.Utterance(text="hello world, how are you?", voice=voice)
            for voice in voices
        ]

        expected_error = (
            "spiel-error-quark",
            int(Spiel.Error.MISCONFIGURED_VOICE),
            "Voice output format not set correctly: 'nuthin'",
        )
        expected_events = [
            ["notify:speaking", True],
            ["utterance-started", one],
            ["utterance-finished", one],
            ["utterance-error", two, expected_error],
            ["utterance-started", three],
            ["utterance-finished", three],
            ["notify:speaking", False],
        ]

        actual_events = self.capture_speak_sequence(speaker, one, two, three)

        self.assertEqual(actual_events, expected_events)


if __name__ == "__main__":
    test_main()
