from _common import *

class SettingsTest(unittest.TestCase):
    def test_default_voice(self):
        settings = Gio.Settings.new("org.monotonous.libspiel")

        self.assertEqual(settings["default-voice"], None)
        settings["default-voice"] = ("ine/hy", "org.mock2.Speech.Provider")
        self.assertEqual(settings["default-voice"], ("ine/hy", "org.mock2.Speech.Provider"))

        self.assertEqual(settings["language-voice-mapping"], {})
        settings["language-voice-mapping"] = {"hy": ("ine/hyw", "org.mock2.Speech.Provider")}
        self.assertEqual(settings["language-voice-mapping"], {"hy": ("ine/hyw", "org.mock2.Speech.Provider")})

if __name__ == "__main__":
    test_main()