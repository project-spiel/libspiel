# Dependencies
python_module = import('python')
py = python_module.find_installation('python3', required: true, modules: [
  'gi',
])

application_id = 'org.monotonous.spiel-it'
app_data_dir = get_option('prefix') / get_option('datadir') / 'spiel-it'
iconsdir = get_option('prefix') / get_option('datadir') / 'icons'
desktop_file_dir = get_option('prefix') / get_option('datadir') / 'applications'
# Configuration used in template files
config = configuration_data()
config.set('app_data_dir', app_data_dir)
config.set('python_path', py.full_path())

configure_file(
  input: 'spiel-it.py.in',
  output: 'spiel-it',
  configuration: config,
  install_mode: 'rwxr-xr-x',
  install_dir: get_option('bindir'),
)

# Python module
install_subdir(
  'spiel_it',
  install_dir: py.get_install_dir(),
)

# ui file
install_data(
  'data/spiel-it.ui',
  install_dir: app_data_dir,
)

#icon
install_data(
  logo,
  install_dir: iconsdir / 'hicolor' / 'symbolic' / 'apps',
  rename: '@0@.svg'.format(application_id)
)

# Desktop file
desktop_conf = configuration_data()
desktop_conf.set('icon', application_id)
configure_file(
  input: 'data/org.monotonous.spiel-it.desktop.in',
  output: 'org.monotonous.spiel-it.desktop',
  configuration: desktop_conf,
  install_dir: desktop_file_dir
)
