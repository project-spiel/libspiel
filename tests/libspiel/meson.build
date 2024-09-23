mock_provider = find_program(meson.current_source_dir() / 'mock_speechprovider.py')
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
  libspiel_test_env.set('ASAN_OPTIONS', 'detect_leaks=1,intercept_tls_get_addr=0')
  libspiel_test_env.set('LSAN_OPTIONS', 'fast_unwind_on_malloc=0,suppressions=@0@'.format(
    join_paths(meson.current_source_dir(), '..', 'lsan.supp')))
  libspiel_test_env.set('SPIEL_TEST', '1')

  test_program = executable(test, '@0@.c'.format(test),
      dependencies: spiel_lib_dep,
  )

  if test_wrapper.found()
    libspiel_test_env.set('G_TEST_EXE', test_program.full_path())
    test_program = test_wrapper
  endif

  test(test, test_program,
            env: libspiel_test_env,
    is_parallel: false,
       protocol: 'tap',
  )
endforeach

