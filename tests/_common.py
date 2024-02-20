import unittest, os, dbus
import dbus.mainloop.glib
from gi.repository import GLib, Gio

import gi

gi.require_version("Spiel", "0.1")
from gi.repository import Spiel

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

STANDARD_VOICES = [
    ["org.mock2.Speech.Provider", "English (Great Britain)", "gmw/en", ["en-gb", "en"]],
    [
        "org.mock2.Speech.Provider",
        "English (Scotland)",
        "gmw/en-GB-scotland#misconfigured",
        ["en-gb-scotland", "en"],
    ],
    [
        "org.mock2.Speech.Provider",
        "English (Lancaster)",
        "gmw/en-GB-x-gbclan",
        ["en-gb-x-gbclan", "en-gb", "en"],
    ],
    ["org.mock2.Speech.Provider", "English (America)", "gmw/en-US", ["en-us", "en"]],
    [
        "org.mock.Speech.Provider",
        "Armenian (East Armenia)",
        "ine/hy",
        ["hy", "hy-arevela"],
    ],
    [
        "org.mock2.Speech.Provider",
        "Armenian (West Armenia)",
        "ine/hyw",
        ["hyw", "hy-arevmda", "hy"],
    ],
    [
        "org.mock.Speech.Provider",
        "Chinese (Cantonese)",
        "sit/yue",
        ["yue", "zh-yue", "zh"],
    ],
    ["org.mock3.Speech.Provider", "Uzbek", "trk/uz", ["uz"]],
]


class BaseSpielTest(unittest.TestCase):
    def __init__(self, *args):
        super().__init__(*args)

    def setUp(self):
        self.mock_service = self.mock_iface("org.mock.Speech.Provider")
        self.mock_service.SetInfinite(False)
        self.mock_service.FlushTasks()

    def tearDown(self):
        settings = Gio.Settings.new("org.monotonous.libspiel")
        settings["default-voice"] = None
        settings["language-voice-mapping"] = {}
        discarded_dir = os.environ["TEST_DISCARDED_SERVICE_DIR"]
        service_dir = os.environ["TEST_SERVICE_DIR"]
        for fname in os.listdir(discarded_dir):
            os.rename(
                os.path.join(discarded_dir, fname), os.path.join(service_dir, fname)
            )

    def mock_iface(self, provider_name):
        session_bus = dbus.SessionBus()
        proxy = session_bus.get_object(
            provider_name,
            f"/{'/'.join(provider_name.split('.'))}",
        )
        return dbus.Interface(
            proxy, dbus_interface="org.freedesktop.Speech.MockProvider"
        )

    def kill_provider(self, provider_name):
        try:
            self.mock_iface(provider_name).KillMe()
        except:
            pass

    def list_active_providers(self):
        session_bus = dbus.SessionBus()
        bus_obj = session_bus.get_object(
            "org.freedesktop.DBus", "/org/freedesktop/DBus"
        )
        iface = dbus.Interface(bus_obj, "org.freedesktop.DBus")
        speech_providers = filter(
            lambda s: s.endswith(".Speech.Provider"),
            iface.ListNames(),
        )
        return [str(s) for s in speech_providers]

    def wait_for_async_speaker_init(self):
        def _init_cb(source, result, user_data):
            user_data.append(Spiel.Speaker.new_finish(result))
            loop.quit()

        speakerContainer = []
        Spiel.Speaker.new(None, _init_cb, speakerContainer)
        loop = GLib.MainLoop()
        loop.run()
        return speakerContainer[0]

    def wait_for_provider_to_go_away(self, name):
        def _cb(*args):
            self.assertNotIn(name, self.list_active_providers())
            session_bus.remove_signal_receiver(
                _cb,
                bus_name="org.freedesktop.DBus",
                dbus_interface="org.freedesktop.DBus",
                signal_name="NameOwnerChanged",
                path="/org/freedesktop/DBus",
                arg0=name,
            )
            GLib.idle_add(loop.quit)

        speech_providers = self.list_active_providers()
        if name not in speech_providers:
            return

        session_bus = dbus.SessionBus()
        sig_match = session_bus.add_signal_receiver(
            _cb,
            bus_name="org.freedesktop.DBus",
            dbus_interface="org.freedesktop.DBus",
            signal_name="NameOwnerChanged",
            path="/org/freedesktop/DBus",
            arg0=name,
        )
        loop = GLib.MainLoop()
        loop.run()

    def wait_for_voices_changed(self, speaker, added=[], removed=[]):
        voices = speaker.props.voices

        def _cb(*args):
            voice_ids = [v.props.identifier for v in voices]
            for a in added:
                if a not in voice_ids:
                    return
            for r in removed:
                if r in voice_ids:
                    return
            voices.disconnect_by_func(_cb)
            loop.quit()

        voices.connect("items-changed", _cb)
        loop = GLib.MainLoop()
        loop.run()

    def wait_for_speaking_done(self, speaker, action):
        def _cb(*args):
            if not speaker.props.speaking:
                speaker.disconnect_by_func(_cb)
                loop.quit()

        speaker.connect("notify::speaking", _cb)
        action()
        loop = GLib.MainLoop()
        loop.run()

    def uninstall_provider(self, name):
        src = os.path.join(
            os.environ["TEST_SERVICE_DIR"], f"{name}{os.path.extsep}service"
        )
        dest = os.path.join(
            os.environ["TEST_DISCARDED_SERVICE_DIR"], f"{name}{os.path.extsep}service"
        )
        os.rename(src, dest)

    def install_provider(self, name):
        src = os.path.join(
            os.environ["TEST_DISCARDED_SERVICE_DIR"], f"{name}{os.path.extsep}service"
        )
        dest = os.path.join(
            os.environ["TEST_SERVICE_DIR"], f"{name}{os.path.extsep}service"
        )
        os.rename(src, dest)

    def get_voice(self, synth, provider_well_known_name, voice_id):
        for v in synth.props.voices:
            if (
                v.props.provider.get_well_known_name() == provider_well_known_name
                and v.props.identifier == voice_id
            ):
                return v

    def capture_speak_sequence(self, speaker, *utterances):
        event_sequence = []

        def _notify_speaking_cb(synth, val):
            event_sequence.append(["notify:speaking", synth.props.speaking])
            if not synth.props.speaking:
                loop.quit()

        def _notify_paused_cb(synth, val):
            event_sequence.append(["notify:paused", synth.props.paused])

        def _utterance_started_cb(synth, utt):
            event_sequence.append(["utterance-started", utt])

        def _utterance_word_started_cb(synth, utt, start, end):
            event_sequence.append(["word-started", utt, start, end])

        def _utterance_sentence_started_cb(synth, utt, start, end):
            event_sequence.append(["sentence-started", utt, start, end])

        def _utterance_canceled_cb(synth, utt):
            event_sequence.append(["utterance-canceled", utt])

        def _utterance_finished_cb(synth, utt):
            event_sequence.append(["utterance-finished", utt])

        def _utterance_error_cb(synth, utt, error):
            event_sequence.append(
                ["utterance-error", utt, (error.domain, error.code, error.message)]
            )
            if not synth.props.speaking:
                loop.quit()

        speaker.connect("notify::speaking", _notify_speaking_cb)
        speaker.connect("notify::paused", _notify_paused_cb)
        speaker.connect("utterance-started", _utterance_started_cb)
        speaker.connect("utterance-canceled", _utterance_canceled_cb)
        speaker.connect("utterance-finished", _utterance_finished_cb)
        speaker.connect("utterance-error", _utterance_error_cb)
        speaker.connect("word-started", _utterance_word_started_cb)
        speaker.connect("sentence-started", _utterance_sentence_started_cb)

        def do_speak():
            for utterance in utterances:
                speaker.speak(utterance)

        GLib.idle_add(do_speak)

        loop = GLib.MainLoop()
        loop.run()

        return event_sequence


def test_main():
    from tap.runner import TAPTestRunner

    runner = TAPTestRunner()
    runner.set_stream(True)
    unittest.main(testRunner=runner)
