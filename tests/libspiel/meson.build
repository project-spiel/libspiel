# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
libspiel_test_deps = [
  spiel_lib_dep,
]

mock_provider = find_program('mock_provider.py')
libspiel_tests = {
  'test-provider': mock_provider,
  'test-speaker': mock_provider,
  'test-utterance': disabler(),
  'test-voice': disabler(),
}

foreach test, test_wrapper : libspiel_tests
  libspiel_test_env = environment()
  libspiel_test_env.set('G_DEBUG', 'fatal-warnings')
  libspiel_test_env.set('GSETTINGS_BACKEND', 'memory')
  libspiel_test_env.set('GSETTINGS_SCHEMA_DIR', join_paths(meson.project_build_root(), 'libspiel'))
  libspiel_test_env.set('SPIEL_TEST', '1')

  test_program = executable(test, '@0@.c'.format(test),
           dependencies: libspiel_test_deps,
            #    install: get_option('installed_tests'),
            #install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  if test_wrapper.found()
    libspiel_test_env.set('G_TEST_EXE', test_program.full_path())
    test_program = test_wrapper
  endif

  test(test, test_program,
           #args: ['--tap'],
            env: libspiel_test_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['libspiel'],
  )
endforeach

