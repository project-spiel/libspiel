import gi
from gi.repository import GLib
import pathlib

gi.require_version("Gtk", "4.0")
gi.require_version("Spiel", "0.1")
gi.require_version("Adw", "1")
gi.require_version("Pango", "1.0")
from gi.repository import Spiel, Gtk, Adw, Gdk, Pango

CSS = """
.overlay-button {
  box-shadow: 2px 2px 2px rgba(0, 0, 0, .2);
}

textview.view {
  box-shadow: inset 2px 0px 2px rgba(0, 0, 0, .2);
}
"""

INITIAL_BUFFER = "I was made to understand there were grilled cheese sandwiches here."

css_provider = Gtk.CssProvider()
css_provider.load_from_string(CSS)
Gtk.StyleContext.add_provider_for_display(
    Gdk.Display.get_default(), css_provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
)

class SpielItApp(Adw.Application):
    def __init__(self, ui_file_path, **kwargs):
        self.ui_file = str(pathlib.Path(ui_file_path) / 'spiel-it.ui')
        super().__init__(**kwargs)
        self.connect("activate", self.on_activate)

    def on_activate(self, app):
        # Need to preload type before loading ui file
        Spiel.Voice()
        self.builder = Gtk.Builder.new_from_file(self.ui_file)
        win = self.builder.get_object("window")
        self.add_window(win)
        self._init()
        win.present()

    def _init(self):
        self.text_view = self.builder.get_object("text-view")
        self.buffer = self.text_view.get_buffer()
        self.stop_button = self.builder.get_object("stop")
        self.playpause_button = self.builder.get_object("play-pause")
        self.voices_select = self.builder.get_object("voices-select")
        self.volume = self.builder.get_object("volume")
        self.rate = self.builder.get_object("rate")
        self.pitch = self.builder.get_object("pitch")

        self.buffer.set_text(INITIAL_BUFFER)

        self.stop_button.connect("clicked", self._on_stop_clicked)
        self.playpause_button.connect("clicked", self._on_playpause_clicked)

        header_factory = Gtk.SignalListItemFactory()
        header_factory.connect("setup", self._on_header_factory_setup)
        header_factory.connect("bind", self._on_header_factory_bind)

        list_factory = Gtk.SignalListItemFactory()
        list_factory.connect("setup", self._on_list_factory_setup)
        list_factory.connect("bind", self._on_list_factory_bind)

        self.voices_select.set_factory(header_factory)
        self.voices_select.set_list_factory(list_factory)

        # spiel
        self.speaker = Spiel.Speaker.new_sync(None)
        self.speaker.connect("notify::speaking", self._on_speaker_update)
        self.speaker.connect("notify::paused", self._on_speaker_update)
        self.speaker.connect("utterance-started", self._on_utterance_started)
        self.speaker.connect("range-started", self._on_range_started_cb)

        self.voices_select.set_model(self.speaker.props.voices)

        self.current_spoken_range = [None, None]
        self.buffer.create_tag("current-range", underline=Pango.Underline.SINGLE)

    def _on_header_factory_setup(self, factory, list_item):
        label = Gtk.Label(max_width_chars=5, ellipsize=Pango.EllipsizeMode.END)
        list_item.set_child(label)

    def _on_header_factory_bind(self, factory, list_item):
        label = list_item.get_child()
        voice = list_item.get_item()
        label.set_text(voice.props.name)

    def _on_list_factory_setup(self, factory, list_item):
        label = Gtk.Label()
        list_item.set_child(label)

    def _on_list_factory_bind(self, factory, list_item):
        label = list_item.get_child()
        voice = list_item.get_item()
        label.set_text(voice.props.name)

    def _on_playpause_clicked(self, button):
        if not self.speaker.props.speaking:
            self._speak()
        elif not self.speaker.props.paused:
            self.speaker.pause()
        else:
            self.speaker.resume()

    def _on_stop_clicked(self, button):
        self.speaker.cancel()

    def _speak(self):
        buffer = self.buffer
        utterance = Spiel.Utterance(
            text=buffer.get_text(buffer.get_start_iter(), buffer.get_end_iter(), False)
        )
        utterance.props.volume = self.volume.get_value()
        utterance.props.rate = self.rate.get_value()
        utterance.props.pitch = self.pitch.get_value()
        utterance.props.voice = self.voices_select.get_selected_item()
        self.speaker.speak(utterance)

    def _on_speaker_update(self, speaker, param):
        if param.name == "speaking":
            self.text_view.set_editable(not speaker.props.speaking)
            if speaker.props.speaking:
                self.playpause_button.set_child(Gtk.Spinner(spinning=True))
            else:
                self.playpause_button.set_icon_name("media-playback-start")
                self.stop_button.set_visible(False)
                begin_iter, end_iter = self.current_spoken_range
                if begin_iter is not None and end_iter is not None:
                    self.buffer.remove_tag_by_name("current-range", begin_iter, end_iter)

        if param.name == "paused" and self.speaker.props.speaking:
            if not self.speaker.props.paused:
                self.playpause_button.set_icon_name("media-playback-pause")
            else:
                self.playpause_button.set_icon_name("media-playback-start")

    def _on_utterance_started(self, speaker, utterance):
        self.playpause_button.set_icon_name("media-playback-pause")
        self.stop_button.set_visible(True)

    def _on_range_started_cb(self, speaker, utterance, begin, end):
        begin_iter, end_iter = self.current_spoken_range
        buffer = self.text_view.get_buffer()
        if begin_iter is not None and end_iter is not None:
            buffer.remove_tag_by_name("current-range", begin_iter, end_iter)
        begin_iter = buffer.get_iter_at_offset(begin)
        end_iter = buffer.get_iter_at_offset(end)
        buffer.apply_tag_by_name("current-range", begin_iter, end_iter)
        self.current_spoken_range = [begin_iter, end_iter]


if __name__ == "__main__":
    import sys

    ui_file = str(pathlib.Path(__file__).parent.parent.resolve() / 'data')

    app = SpielItApp(ui_file)
    app.run(sys.argv)
