toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())
toml_conf.set('license', meson.project_license())

source_toml = configure_file(
  input: 'Spiel.toml.in',
  output: 'Spiel.toml',
  configuration: toml_conf,
  # install: true,
  # install_dir: docs_dir / 'libspiel',
)

logo = join_paths(meson.project_source_root(), 'spiel-logo.svg')

fs = import('fs')
urlmap_doc = fs.copyfile('urlmap.js')
logo_file = fs.copyfile(logo)

gidocgen = find_program('gi-docgen', required: true)

gidocgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
]

if get_option('werror')
  gidocgen_common_args += ['--fatal-warnings']
endif

python_module = import('python')
python = python_module.find_installation(
  'python3', required : true)
overview_doc = custom_target(
    input: join_paths(meson.project_source_root(), 'README.md'),
    output: 'overview.md',
    command: [python, files('generate_overview.py'), '@INPUT@'],
    capture: true
)

custom_target('libspiel-doc',
  input: [ source_toml, spiel_gir[0] ],
  output: 'libspiel',
  command: [
    gidocgen,
    'generate',
    gidocgen_common_args,
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_build_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  depends: [ overview_doc, urlmap_doc, logo_file ],
  install: true,
  install_dir: get_option('datadir') / 'doc',
)
