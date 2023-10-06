from _common import *

class SettingsTest(unittest.TestCase):
    def test_default_voice(self):
        settings = Gio.Settings.new("org.monotonous.libspiel")

        self.assertEqual(settings["default-voice"], "")
        settings["default-voice"] = "alice"
        self.assertEqual(settings["default-voice"], "alice")

        self.assertEqual(settings["language-voice-mapping"], {})
        settings["language-voice-mapping"] = {"en-US": "bob"}
        self.assertEqual(settings["language-voice-mapping"], {"en-US": "bob"})

if __name__ == "__main__":
    test_main()