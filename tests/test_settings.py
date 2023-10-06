from _common import *


class SettingsTest(unittest.TestCase):
    def test_default_voice(self):
        settings = Gio.Settings.new("org.monotonous.libspiel")

        self.assertEqual(settings["default-voice"], None)
        settings["default-voice"] = ("org.mock2.Speech.Provider", "ine/hy")
        self.assertEqual(
            settings["default-voice"], ("org.mock2.Speech.Provider", "ine/hy")
        )

        self.assertEqual(settings["language-voice-mapping"], {})
        settings["language-voice-mapping"] = {
            "hy": ("org.mock2.Speech.Provider", "ine/hyw")
        }
        self.assertEqual(
            settings["language-voice-mapping"],
            {"hy": ("org.mock2.Speech.Provider", "ine/hyw")},
        )


if __name__ == "__main__":
    test_main()
