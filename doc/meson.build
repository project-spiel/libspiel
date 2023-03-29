toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())

gidocgen = find_program('gi-docgen', required: true)

gidocgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
]

if get_option('werror')
  gidocgen_common_args += ['--fatal-warnings']
endif

# docs_dir = datadir / 'doc'

source_toml = configure_file(
  input: 'Jsonrpc.toml.in',
  output: 'Jsonrpc.toml',
  configuration: toml_conf,
  # install: true,
  # install_dir: docs_dir / 'libspiel',
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
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  # install: true,
  # install_dir: docs_dir,
)
