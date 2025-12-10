fs = import('fs')

service_dir = join_paths(meson.current_build_dir(), 'services')

test_env = environment()

test_env.prepend('GI_TYPELIB_PATH', meson.project_build_root() / 'libspiel', separator: ':')
test_env.prepend('LD_LIBRARY_PATH', meson.project_build_root() / 'libspiel', separator: ':')

speechprovider_libdir = speechprovider_dep.get_variable('libdir')
test_env.prepend('LD_LIBRARY_PATH', speechprovider_libdir, separator: ':')
if fs.is_dir(speechprovider_libdir / 'girepository-1.0')
  test_env.prepend('GI_TYPELIB_PATH', speechprovider_libdir / 'girepository-1.0', separator: ':')
else
  test_env.prepend('GI_TYPELIB_PATH', speechprovider_libdir, separator: ':')
endif

test_env.set('G_DEBUG', 'fatal-warnings')
test_env.set('GSETTINGS_SCHEMA_DIR', join_paths(meson.project_build_root(), 'libspiel'))
test_env.set('GSETTINGS_BACKEND', 'memory')
test_env.set('SPIEL_TEST', '1')

python_module = import('python')
dbus_run_session = find_program('dbus-run-session', required : false)
python = python_module.find_installation(
  'python3', required : false, modules: ['tap', 'dbusmock'])

test_deps = spiel_deps + [
  spiel_lib_dep,
]

if python.found() and introspection
    tests = [
         'test_speak.py',
         'test_types.py',
         'test_voices.py',
         'test_settings.py',
         'test_voice_selection.py',
         'test_errors.py',
         'test_audio.py',
         'test_provider.py'
    ]

    foreach test_name : tests
      test(
        test_name, python,
        args : [files(test_name)],
        env : test_env,
        protocol : 'tap',
        depends: [spiel_gir, spiel_schema],
        is_parallel : test_name != 'test_provider.py'
      )
    endforeach
endif

native_tests = [
  'test-simple-speak',
  'test-simultaneous-init',
]

foreach test_name : native_tests
  test_exec = executable(
    test_name,
    '@0@.c'.format(test_name),
    dependencies: test_deps
  )

  test(test_name, dbus_run_session,
        args : [
          python.full_path(),
          '-m',
          'dbusmock',
          '--template',
          join_paths(meson.current_source_dir(), 'speechprovider.py'),
          '-e', test_exec.full_path(),
          ],
        env : test_env
      )
endforeach
