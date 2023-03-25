import unittest, os, dbus
import dbus.mainloop.glib
from gi.repository import GLib

import gi

gi.require_version("Spiel", "0.1")
from gi.repository import Spiel

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

STANDARD_VOICES = [
    [
        "English (Great Britain)",
        "gmw/en",
        ["en-gb", "en"],
        "org.freedesktop.Speech.Synthesis.Mock3",
    ],
    [
        "Armenian (East Armenia)",
        "ine/hy",
        ["hy", "hy-arevela"],
        "org.freedesktop.Speech.Synthesis.Mock",
    ],
    [
        "Armenian (West Armenia)",
        "ine/hyw",
        ["hyw", "hy-arevmda", "hy"],
        "org.freedesktop.Speech.Synthesis.Mock2",
    ],
    [
        "Chinese (Cantonese)",
        "sit/yue",
        ["yue", "zh-yue", "zh"],
        "org.freedesktop.Speech.Synthesis.Mock",
    ],
]

    
class BaseSpielTest(unittest.TestCase):
    def __init__(self, *args):
        self.mock_service = self.mock_iface("org.freedesktop.Speech.Synthesis.Mock")
        super().__init__(*args)

    def setUp(self):
        self.mock_service.SetAutoStep(True)
        self.mock_service.FlushTasks()

    def tearDown(self):
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
            proxy, dbus_interface="org.freedesktop.Speech.Synthesis.MockSpeaker"
        )

    def kill_provider(self, provider_name):
        try:
            self.mock_iface(provider_name).KillMe()
        except:
            pass

    def list_active_providers(self):
        def _cb(*args):
            pass

        session_bus = dbus.SessionBus()
        bus_obj = session_bus.get_object(
            "org.freedesktop.DBus", "/org/freedesktop/DBus"
        )
        iface = dbus.Interface(bus_obj, "org.freedesktop.DBus")
        iface.connect_to_signal(
            "NameOwnerChanged", _cb, arg0="org.freedesktop.Speech.Synthesis.Mock3"
        )
        speech_providers = filter(
            lambda s: s.startswith("org.freedesktop.Speech.Synthesis."),
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
            loop.quit()

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

    def wait_for_voices_changed(self, speaker):
        def _cb(*args):
            speaker.disconnect_by_func(_cb)
            loop.quit()

        speaker.connect("notify::voices", _cb)
        loop = GLib.MainLoop()
        loop.run()

    def wait_for_speaking_changed(self, speaker, action):
        def _cb(*args):
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


def test_main():
    from tap.runner import TAPTestRunner

    runner = TAPTestRunner()
    runner.set_stream(True)
    unittest.main(testRunner=runner)
