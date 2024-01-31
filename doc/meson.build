toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())

source_toml = configure_file(
  input: 'Spiel.toml.in',
  output: 'Spiel.toml',
  configuration: toml_conf,
  # install: true,
  # install_dir: docs_dir / 'libspiel',
)

interface_src_dir = join_paths(meson.project_source_root(), 'interface')
logo = join_paths(meson.project_source_root(), 'spiel-logo.svg')

gdbus_codegen_cmd = find_program('gdbus-codegen')
spiel_iface_docs = custom_target('dbus-docs',
  output : 'generated-org.freedesktop.Speech.Provider.md',
  input : join_paths(interface_src_dir, 'org.freedesktop.Speech.Provider.xml'),
  command : [gdbus_codegen_cmd, '--generate-md', 'generated',
             '--output-directory', meson.current_build_dir(),
             join_paths(interface_src_dir, 'org.freedesktop.Speech.Provider.xml')])

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
  'python3', required : true, modules: 'tap')
overview_doc = custom_target(
    input: join_paths(meson.project_source_root(), 'README.md'),
    output: 'overview.md',
    command: [python, files('generate_overview.py'), '@INPUT@'],
    capture: true
)

if get_option('libspiel')
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
    depends: [ spiel_iface_docs, overview_doc, urlmap_doc, logo_file ]
    # install: true,
    # install_dir: docs_dir,
  )
endif