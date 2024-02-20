python_module = import('python')
python = python_module.find_installation(
  'python3', required : true)
gen_enums = files(join_paths(meson.project_source_root(), 'interface', 'generate_enums.py'))
speech_provider_dbus_enums_h = custom_target(
    input: join_paths(meson.project_source_root(), 'interface', 'org.freedesktop.Speech.Provider.xml'),
    output: 'speech-provider-dbus-enums.h',
    command: [python, gen_enums, 'speech_provider', '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('includedir'), 'speech-provider')
)
speech_provider_dbus_enums_c = custom_target(
    input: join_paths(meson.project_source_root(), 'interface', 'org.freedesktop.Speech.Provider.xml'),
    output: 'speech-provider-dbus-enums.c',
    command: [python, gen_enums, 'speech_provider', '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('includedir'), 'speech-provider')
)



speech_provider_sources = [
  'speech-provider-stream-reader.c',
  'speech-provider-stream-writer.c',
  speech_provider_dbus_enums_c,
]

speech_provider_headers = [
  'speech-provider.h',
  'speech-provider-common.h',
  'speech-provider-stream-reader.h',
  'speech-provider-stream-writer.h',
]

version_split = meson.project_version().split('.')
version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', version_split[0])
version_conf.set('MINOR_VERSION', version_split[1])
version_conf.set('MICRO_VERSION', version_split[2])

speech_provider_version_h = configure_file(
          input: 'speech-provider-version.h.in',
         output: 'speech-provider-version.h',
  configuration: version_conf,
        install: true,
    install_dir: join_paths(get_option('includedir'), 'speech-provider')
)

speech_provider_lib_generated = [
  speech_provider_version_h,
  speech_provider_dbus_enums_h,
  speech_provider_dbus_enums_c,
]

speech_provider_deps = [
  dependency('gobject-2.0'),
]

speech_provider_lib = shared_library('speech-provider-' + api_version,
  [speech_provider_sources, speech_provider_lib_generated],
  dependencies: speech_provider_deps,
  install: true,
)

speech_provider_lib_dep = declare_dependency(
  sources: speech_provider_lib_generated,
  dependencies: speech_provider_deps,
  link_with: speech_provider_lib,
  include_directories: include_directories('.'),
)

install_headers(speech_provider_headers, subdir: 'speech-provider')

pkg = import('pkgconfig')
pkg.generate(
  description: 'A shared library for speech synthesis clients',
    libraries: speech_provider_lib,
         name: 'speech-provider',
     filebase: 'speech-provider-' + api_version,
      version: meson.project_version(),
      subdirs: 'speech-provider',
     requires: 'gobject-2.0'
)

gnome = import('gnome')
speech_provider_gir = gnome.generate_gir(speech_provider_lib,
            sources: speech_provider_headers + speech_provider_sources + speech_provider_dbus_enums_h,
            nsversion: api_version,
            namespace: 'SpeechProvider',
            header: 'speech-provider/speech-provider.h',
        symbol_prefix: 'speech_provider',
    identifier_prefix: 'SpeechProvider',
             includes: [ 'GObject-2.0' ],
              install: true,
              export_packages: 'speech-provider',
)
