project('spiel', 'c',
          version: '1.0.5',
    meson_version: '>= 0.64.0',
  default_options: [ 'warning_level=2', 'werror=false', 'c_std=gnu11', ],
  license: 'LGPL-2.1-or-later',
)

api_version = '1.0'

cc = meson.get_compiler('c')

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
configure_file(output: 'config.h', configuration: config_h)
add_project_arguments(['-I' + meson.project_build_root()], language: 'c')

project_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  '-Werror=address',
  '-Werror=array-bounds',
  '-Werror=empty-body',
  '-Werror=implicit',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=int-to-pointer-cast',
  '-Werror=main',
  '-Werror=misleading-indentation',
  '-Werror=missing-braces',
  '-Werror=missing-include-dirs',
  '-Werror=nonnull',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=pointer-arith',
  '-Werror=pointer-to-int-cast',
  '-Werror=redundant-decls',
  '-Werror=return-type',
  '-Werror=sequence-point',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',
  '-Wformat-nonliteral',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-dangling-pointer',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-fno-strict-aliasing',
  ['-Werror=format-security', '-Werror=format=2'],
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif
foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    project_c_args += arg
  endif
endforeach
add_project_arguments(project_c_args, language: 'c')

# Synced dependency versions
glib_version = '>= 2.76'
gst_version = '>= 1.0'
speechprovider_version = '>= 1.0.3'

introspection = get_option('introspection')

speechprovider_options = ['docs=false']
if not introspection
  speechprovider_options += ['introspection=false']
endif

speechprovider_dep = dependency('speech-provider-1.0', version: speechprovider_version, required : false)
if speechprovider_dep.found()
  iface_xml = speechprovider_dep.get_variable('iface_xml')
  iface_xml_path = iface_xml
  system_installed_speechprovider = true
else
  speechprovider_proj = subproject('libspeechprovider', default_options: speechprovider_options)
  iface_xml = speechprovider_proj.get_variable('iface_xml')
  iface_xml_path = iface_xml.full_path()
  speechprovider_dep = speechprovider_proj.get_variable('speech_provider_lib_dep')
  system_installed_speechprovider = false
endif


subdir('libspiel')

if get_option('tests')
  subdir('tests')
endif

if get_option('utils')
  subdir('utils')
endif

if get_option('docs') and introspection
  subdir('doc')
endif

