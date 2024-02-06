python_module = import('python')
python = python_module.find_installation(
  'python3', required : true)
gen_enums = files(join_paths(meson.project_source_root(), 'interface', 'generate_enums.py'))
spiel_provider_dbus_enums_h = custom_target(
    input: join_paths(meson.project_source_root(), 'interface', 'org.freedesktop.Speech.Provider.xml'),
    output: 'spiel-provider-dbus-enums.h',
    command: [python, gen_enums, 'spiel_provider', '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('includedir'), 'spiel-provider')
)
spiel_provider_dbus_enums_c = custom_target(
    input: join_paths(meson.project_source_root(), 'interface', 'org.freedesktop.Speech.Provider.xml'),
    output: 'spiel-provider-dbus-enums.c',
    command: [python, gen_enums, 'spiel_provider', '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('includedir'), 'spiel-provider')
)



spiel_provider_sources = [
  'spiel-provider-stream-reader.c',
  'spiel-provider-stream-writer.c',
  spiel_provider_dbus_enums_c,
]

spiel_provider_headers = [
  'spiel-provider.h',
  'spiel-provider-common.h',
  'spiel-provider-stream-reader.h',
  'spiel-provider-stream-writer.h',
]

version_split = meson.project_version().split('.')
version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', version_split[0])
version_conf.set('MINOR_VERSION', version_split[1])
version_conf.set('MICRO_VERSION', version_split[2])

spiel_provider_version_h = configure_file(
          input: 'spiel-provider-version.h.in',
         output: 'spiel-provider-version.h',
  configuration: version_conf,
        install: true,
    install_dir: join_paths(get_option('includedir'), 'spiel-provider')
)

spiel_provider_lib_generated = [
  spiel_provider_version_h,
  spiel_provider_dbus_enums_h,
  spiel_provider_dbus_enums_c,
]

spiel_provider_deps = [
  dependency('gobject-2.0'),
]

spiel_provider_lib = shared_library('spiel-provider-' + api_version,
  spiel_provider_sources,
  dependencies: spiel_provider_deps,
  install: true,
)

spiel_provider_lib_dep = declare_dependency(
  sources: spiel_provider_lib_generated,
  dependencies: spiel_provider_deps,
  link_with: spiel_provider_lib,
  include_directories: include_directories('.'),
)

install_headers(spiel_provider_headers, subdir: 'spiel-provider')

pkg = import('pkgconfig')
pkg.generate(
  description: 'A shared library for speech synthesis clients',
    libraries: spiel_provider_lib,
         name: 'spiel-provider',
     filebase: 'spiel-provider-' + api_version,
      version: meson.project_version(),
      subdirs: 'spiel-provider',
     requires: 'gobject-2.0'
)

gnome = import('gnome')
spiel_provider_gir = gnome.generate_gir(spiel_provider_lib,
            sources: spiel_provider_headers + spiel_provider_sources + spiel_provider_dbus_enums_h,
            nsversion: api_version,
            namespace: 'SpielProvider',
            header: 'spiel-provider/spiel-provider.h',
        symbol_prefix: 'spiel_provider',
    identifier_prefix: 'SpielProvider',
             includes: [ 'GObject-2.0' ],
              install: true,
              export_packages: 'spiel-provider',
)
