import unittest, os, dbus
import dbus.mainloop.glib
from gi.repository import GLib, Gio
import dbusmock
import gi
import sys
import json
from pathlib import Path

gi.require_version("Spiel", "1.0")
from gi.repository import Spiel

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

LOG_EVENTS = False

STANDARD_VOICES = [
    ["org.two.Speech.Provider", "English (Great Britain)", "gmw/en", ["en-gb", "en"]],
    [
        "org.two.Speech.Provider",
        "English (Scotland)",
        "gmw/en-GB-scotland#misconfigured",
        ["en-gb-scotland", "en"],
    ],
    [
        "org.two.Speech.Provider",
        "English (Lancaster)",
        "gmw/en-GB-x-gbclan",
        ["en-gb-x-gbclan", "en-gb", "en"],
    ],
    ["org.two.Speech.Provider", "English (America)", "gmw/en-US", ["en-us", "en"]],
    [
        "org.one.Speech.Provider",
        "Armenian (East Armenia)",
        "ine/hy",
        ["hy", "hy-arevela"],
    ],
    [
        "org.two.Speech.Provider",
        "Armenian (West Armenia)",
        "ine/hyw",
        ["hyw", "hy-arevmda", "hy"],
    ],
    [
        "org.one.Speech.Provider",
        "Chinese (Cantonese)",
        "sit/yue",
        ["yue", "zh-yue", "zh"],
    ],
    ["org.three.Speech.Provider", "Uzbek", "trk/uz", ["uz"]],
]


class BaseSpielTest(dbusmock.DBusTestCase):
    @classmethod
    def setUpClass(cls):
        cls.start_session_bus()
        cls.dbus_con = cls.get_dbus()
        if os.getenv("SPIEL_TEST_DBUS_MONITOR"):
            cls.dbus_monitor = subprocess.Popen(["dbus-monitor", "--session"])

        cls.install_providers(
            [
                "org.one.Speech.Provider",
                "org.two.Speech.Provider",
                "org.three.Speech.Provider",
            ]
        )

    def __init__(self, *args):
        self._mocks = {}
        super().__init__(*args)

    def setUp(self):
        pass

    def tearDown(self):
        settings = Gio.Settings.new("org.monotonous.libspiel")
        settings["default-voice"] = None
        settings["language-voice-mapping"] = {}
        for mock in self._mocks.values():
            try:
                mock.SetInfinite(False)
                mock.FlushTasks()
            except dbus.exceptions.DBusException:
                pass
        self._mocks = {}

    def mock_iface(self, provider_name):
        if provider_name in self._mocks:
            return self._mocks[provider_name]
        session_bus = dbus.SessionBus()
        proxy = session_bus.get_object(
            provider_name,
            f"/{'/'.join(provider_name.split('.'))}",
        )
        mock = dbus.Interface(
            proxy, dbus_interface="org.freedesktop.Speech.Provider.Mock"
        )
        self._mocks[provider_name] = mock
        return mock

    def kill_provider(self, provider_name):
        try:
            self.mock_iface(provider_name).Die()
            self._mocks.pop(provider_name)
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

    @classmethod
    def uninstall_providers(cls, names):
        services_dir = cls.get_services_dir()
        for name in names:
            (Path(services_dir) / f"{name}.service").unlink()

        dbus_obj = cls.dbus_con.get_object(
            "org.freedesktop.DBus", "/org/freedesktop/DBus"
        )
        dbus_if = dbus.Interface(dbus_obj, "org.freedesktop.DBus")
        dbus_if.ReloadConfig()

    @classmethod
    def install_providers(cls, names):
        services_dir = cls.get_services_dir()
        for name in names:
            template_full_path = Path(__file__).absolute().parent / "speechprovider.py"
            params = {"name": name}
            file_contents = [
                "[D-BUS Service]",
                f"Name={name}",
                f"Exec={sys.executable} -m dbusmock -l /tmp/{name}.txt --template {template_full_path} -p '{json.dumps(params)}'",
            ]
            f = open(os.path.join(services_dir, f"{name}.service"), "w")
            f.write("\n".join(file_contents) + "\n")
            f.close()

        dbus_obj = cls.dbus_con.get_object(
            "org.freedesktop.DBus", "/org/freedesktop/DBus"
        )
        dbus_if = dbus.Interface(dbus_obj, "org.freedesktop.DBus")
        dbus_if.ReloadConfig()

    def get_voice(self, synth, provider_identifier, voice_id):
        for v in synth.props.voices:
            if (
                v.props.provider.get_identifier() == provider_identifier
                and v.props.identifier == voice_id
            ):
                return v

    def capture_speak_sequence(self, speaker, *utterances):
        event_sequence = []

        def _append_to_sequence(signal_and_args):
            event_sequence.append(signal_and_args)
            if LOG_EVENTS:
                print(signal_and_args)

        def _notify_speaking_cb(synth, val):
            _append_to_sequence(["notify:speaking", synth.props.speaking])
            if not synth.props.speaking:
                loop.quit()

        def _notify_paused_cb(synth, val):
            _append_to_sequence(["notify:paused", synth.props.paused])

        def _utterance_started_cb(synth, utt):
            _append_to_sequence(["utterance-started", utt])

        def _utterance_word_started_cb(synth, utt, start, end):
            _append_to_sequence(["word-started", utt, start, end])

        def _utterance_sentence_started_cb(synth, utt, start, end):
            _append_to_sequence(["sentence-started", utt, start, end])

        def _utterance_canceled_cb(synth, utt):
            _append_to_sequence(["utterance-canceled", utt])

        def _utterance_finished_cb(synth, utt):
            _append_to_sequence(["utterance-finished", utt])

        def _utterance_error_cb(synth, utt, error):
            _append_to_sequence(
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
