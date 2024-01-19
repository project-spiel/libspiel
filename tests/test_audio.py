from _common import *

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst

Gst.init(None)


class TestSpeak(BaseSpielTest):
    def _setup_custom_sink(self):
        bin = Gst.Bin.new("bin")
        level = Gst.ElementFactory.make("level", "level")
        bin.add(level)
        sink = Gst.ElementFactory.make("fakesink", "sink")
        bin.add(sink)
        level.link(sink)

        level.set_property("post-messages", True)
        # level.set_property("interval", 100)
        sink.set_property("sync", True)

        pad = level.get_static_pad("sink")
        ghostpad = Gst.GhostPad.new("sink", pad)
        bin.add_pad(ghostpad)

        speechSynthesis = Spiel.Speaker.new_sync(None)
        speechSynthesis.props.sink = bin

        pipeline = bin.get_parent()
        bus = pipeline.get_bus()

        return speechSynthesis, bus

    def test_max_volume(self):
        loop = GLib.MainLoop()

        def _on_message(bus, message):
            info = message.get_structure()
            if info.get_name() == "level":
                self.assertGreater(info.get_value("rms")[0], -5)
                loop.quit()

        speechSynthesis, bus = self._setup_custom_sink()

        bus.connect("message::element", _on_message)

        utterance = Spiel.Utterance(text="hello world, how are you?")
        utterance.props.volume = 1
        speechSynthesis.speak(utterance)

        loop.run()

    def test_half_volume(self):
        loop = GLib.MainLoop()

        def _on_message(bus, message):
            info = message.get_structure()
            if info.get_name() == "level":
                self.assertLess(info.get_value("rms")[0], -5)
                loop.quit()

        speechSynthesis, bus = self._setup_custom_sink()

        bus.connect("message::element", _on_message)

        utterance = Spiel.Utterance(text="hello world, how are you?")
        utterance.props.volume = 0.5
        speechSynthesis.speak(utterance)

        loop.run()


if __name__ == "__main__":
    test_main()
