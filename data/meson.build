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
