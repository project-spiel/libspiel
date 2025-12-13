gnome = import('gnome')

spiel_iface_sources = gnome.gdbus_codegen(
    'spiel-provider-proxy',
    iface_xml,
    interface_prefix: 'org.freedesktop.Speech.',
    namespace: 'Spiel',
    annotations : [
      ['org.freedesktop.Speech.Provider', 'org.gtk.GDBus.C.Name', 'ProviderProxy']
    ],
    extra_args: '--glib-min-required=2.64')

python_module = import('python')
python = python_module.find_installation(
  'python3', required : true)
generate_enums = files(meson.current_source_dir() / 'generate_enums.py')
spiel_dbus_enums_ch = custom_target(
    output: ['spiel-dbus-enums.c', 'spiel-dbus-enums.h'],
    command: [python, generate_enums, 'spiel', iface_xml, '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('includedir'), 'spiel')
)

spiel_public_sources = [
  'spiel-utterance.c',
  'spiel-voice.c',
  'spiel-speaker.c',
  'spiel-provider.c',
]

spiel_sources = [
  spiel_public_sources,
  'spiel-registry.c',
  'spiel-providers-list-model.c',
  'spiel-portal-helpers.c',
  'spiel-provider-src.c',
  'spiel-voices-list-model.c',
  spiel_iface_sources,
  spiel_dbus_enums_ch[0],
]

spiel_public_headers = [
  'spiel.h',
  'spiel-utterance.h',
  'spiel-voice.h',
  'spiel-speaker.h',
  'spiel-provider.h',
]

spiel_headers = [
  spiel_public_headers,
  'spiel-registry.h',
  'spiel-providers-list-model.h',
  'spiel-portal-helpers.h',
  'spiel-voices-list-model.h',
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
  spiel_dbus_enums_ch,
]

spiel_deps = [
  dependency('gio-2.0', version: glib_version),
  dependency('gio-unix-2.0', version: glib_version),
  dependency('gstreamer-1.0', version: gst_version),
  dependency('gstreamer-audio-1.0', version: gst_version),
  speechprovider_dep,
]

spiel_lib = shared_library('spiel-' + api_version,
  spiel_sources,
  dependencies: spiel_deps,
  version: meson.project_version(),
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

if introspection
  spiel_gir = gnome.generate_gir(spiel_lib,
              sources: spiel_public_headers + spiel_public_sources + spiel_dbus_enums_ch[1],
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

spiel_schema = gnome.compile_schemas(build_by_default: true, depend_files: 'org.monotonous.libspiel.gschema.xml')

gnome.post_install(glib_compile_schemas: true)

compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  # meson.add_install_script('glib-compile-schemas', join_paths(get_option('prefix'), schemas_dir))
  test('Validate schema file', compile_schemas,
    args: ['--strict', '--dry-run', meson.current_source_dir()]
  )
endif

