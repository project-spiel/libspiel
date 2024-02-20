gnome = import('gnome')

spiel_iface_sources = gnome.gdbus_codegen(
    'spiel-provider-proxy',
    join_paths(meson.project_source_root(), 'interface', 'org.freedesktop.Speech.Provider.xml'),
    interface_prefix: 'org.freedesktop.Speech.',
    namespace: 'Spiel',
    annotations : [
      ['org.freedesktop.Speech.Provider', 'org.gtk.GDBus.C.Name', 'ProviderProxy']
    ],
    extra_args: '--glib-min-required=2.64')

python_module = import('python')
python = python_module.find_installation(
  'python3', required : true)
gen_enums = files(join_paths(meson.project_source_root(), 'interface', 'generate_enums.py'))
spiel_dbus_enums_h = custom_target(
    input: join_paths(meson.project_source_root(), 'interface', 'org.freedesktop.Speech.Provider.xml'),
    output: 'spiel-dbus-enums.h',
    command: [python, gen_enums, 'spiel', '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('includedir'), 'spiel')
)
spiel_dbus_enums_c = custom_target(
    input: join_paths(meson.project_source_root(), 'interface', 'org.freedesktop.Speech.Provider.xml'),
    output: 'spiel-dbus-enums.c',
    command: [python, gen_enums, 'spiel', '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('includedir'), 'spiel')
)

spiel_public_sources = [
  'spiel-utterance.c',
  'spiel-voice.c',
  'spiel-speaker.c',
  'spiel-provider.c',
  'spiel-voices-list-model.c',
]

spiel_sources = [
  spiel_public_sources,
  'spiel-registry.c',
  'spiel-collect-providers.c',
  'spiel-provider-src.c',
  spiel_iface_sources,
  spiel_dbus_enums_c,
]

spiel_public_headers = [
  'spiel.h',
  'spiel-utterance.h',
  'spiel-voice.h',
  'spiel-speaker.h',
  'spiel-provider.h',
  'spiel-voices-list-model.h',
]

spiel_headers = [
  spiel_public_headers,
  'spiel-registry.h',
  'spiel-collect-providers.h',
  'spiel-provider-src.h',
  'spiel-provider-private.h',
]

version_split = meson.project_version().split('.')
version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', version_split[0])
version_conf.set('MINOR_VERSION', version_split[1])
version_conf.set('MICRO_VERSION', version_split[2])

spiel_version_h = configure_file(
          input: 'spiel-version.h.in',
         output: 'spiel-version.h',
  configuration: version_conf,
        install: true,
    install_dir: join_paths(get_option('includedir'), 'spiel')
)

spiel_lib_generated = [
  spiel_version_h,
  spiel_dbus_enums_h,
  spiel_dbus_enums_c,
]

spiel_deps = [
  dependency('gio-2.0'),
  dependency('gio-unix-2.0'),
  dependency('gstreamer-1.0'),
  dependency('gstreamer-audio-1.0'),
  speech_provider_lib_dep
]

spiel_lib = shared_library('spiel-' + api_version,
  spiel_sources,
  dependencies: spiel_deps,
  include_directories: include_directories('../interface'),
  install: true,
)

spiel_lib_dep = declare_dependency(
  sources: spiel_lib_generated,
  dependencies: spiel_deps,
  link_with: spiel_lib,
  include_directories: include_directories('.'),
)

install_headers(spiel_public_headers, subdir: 'spiel')

pkg = import('pkgconfig')
pkg.generate(
  description: 'A shared library for speech synthesis clients',
    libraries: spiel_lib,
         name: 'spiel',
     filebase: 'spiel-' + api_version,
      version: meson.project_version(),
      subdirs: 'spiel',
     requires: 'gio-2.0'
)

if get_option('docs') or get_option('introspection')
spiel_gir = gnome.generate_gir(spiel_lib,
              sources: spiel_public_headers + spiel_public_sources + spiel_dbus_enums_h,
              nsversion: api_version,
              namespace: 'Spiel',
              header: 'spiel/spiel.h',
          symbol_prefix: 'spiel',
      identifier_prefix: 'Spiel',
               includes: [ 'Gio-2.0' ],
                install: true,
                export_packages: 'spiel',
  )
endif

schemas_dir = join_paths(get_option('datadir'), 'glib-2.0/schemas')
install_data('org.monotonous.libspiel.gschema.xml',
  install_dir: schemas_dir
)

gnome.compile_schemas(build_by_default: true, depend_files: 'org.monotonous.libspiel.gschema.xml')

gnome.post_install(glib_compile_schemas: true)

compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  # meson.add_install_script('glib-compile-schemas', join_paths(get_option('prefix'), schemas_dir))
  test('Validate schema file', compile_schemas,
    args: ['--strict', '--dry-run', meson.current_source_dir()]
  )
endif
