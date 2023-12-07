service_dir = join_paths(meson.current_build_dir(), 'services')

test_env = environment(test_env_vars)
test_env.set('G_DEBUG', 'fatal-warnings')
test_env.set('TEST_SERVICE_DIR', service_dir)
test_env.set('TEST_DISCARDED_SERVICE_DIR', join_paths(meson.current_build_dir(), 'discarded_services'))
test_env.set('GSETTINGS_SCHEMA_DIR', join_paths(meson.project_build_root(), 'data'))
test_env.set('GSETTINGS_BACKEND', 'memory')

python_module = import('python')
dbus_run_session = find_program('dbus-run-session', required : false)
python = python_module.find_installation(
  'python3', required : false, modules: 'tap')

tests = ['test_speak.py', 'test_types.py', 'test_voices.py', 'test_settings.py', 'test_voice_selection.py']

newer_dbus = dependency('dbus-1', version : '>=1.14.4', required : false)
if newer_dbus.found()
  tests += ['test_install_provider.py']
endif

conf = configuration_data()
conf.set('service_dir', service_dir)
test_bus_conf_file = configure_file(
  input: 'test-bus.conf.in',
  output: 'test-bus.conf',
  configuration: conf)

test_deps = spiel_deps + [
  spiel_lib_dep,
]

test_simple_speak = executable('test-simple-speak', 'test-simple-speak.c',
  dependencies: test_deps
)

if dbus_run_session.found() and python.found()
  foreach test_name : tests
    test(
      test_name, dbus_run_session,
      args : [
        '--config-file=@0@'.format(join_paths(meson.current_build_dir(), 'test-bus.conf')),
        '--',
        python.full_path(),
        files(test_name)
      ],
      env : test_env,
      protocol : 'tap',
      is_parallel : test_name != 'test_install_provider.py'
    )
  endforeach

test('test-simple-speak', dbus_run_session,
      args : [
        '--config-file=@0@'.format(join_paths(meson.current_build_dir(), 'test-bus.conf')),
        '--', test_simple_speak
        ],
      env : test_env
    )
endif


subdir('services')
subdir('discarded_services')