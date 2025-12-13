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
         'test_speak',
         'test_types',
         'test_voices',
         'test_settings',
         'test_voice_selection',
         'test_errors',
         'test_audio',
         'test_provider'
    ]

    foreach test_name : tests
      test(
        'direct/@0@'.format(test_name), python,
        args : [files('@0@.py'.format(test_name))],
        env : test_env,
        protocol : 'tap',
        depends: [spiel_gir, spiel_schema],
        is_parallel : test_name != 'test_provider.py'
      )
    endforeach

    if get_option('portal-tests')
      desktop_portal = subproject('xdg-desktop-portal', default_options: [
        'tests=disabled',
        'documentation=disabled',
        'geoclue=disabled',
        'man-pages=disabled']
      )

      xdg_desktop_portal_exec = meson.project_build_root() / 'subprojects' / 'xdg-desktop-portal' / 'src' / 'xdg-desktop-portal'
      portal_env = test_env
      portal_env.set('SPIEL_USE_PORTAL', '1')
      portal_env.set('XDG_DESKTOP_PORTAL_EXECUTABLE', xdg_desktop_portal_exec)

      foreach test_name : tests
        test(
          'portal/@0@'.format(test_name), python,
          args : [files('@0@.py'.format(test_name))],
          env : portal_env,
          protocol : 'tap',
          depends: [spiel_gir, spiel_schema],
          is_parallel : test_name != 'test_provider.py'
        )
      endforeach
    endif

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

  test('native/@0@'.format(test_name), dbus_run_session,
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
