conf = configuration_data()
conf.set('service_exec_dir', join_paths(meson.current_source_dir()))

service_names = ['mock', 'mock2', 'mock3']

foreach service_name : service_names
    c = configuration_data({
        'service_exec_dir': meson.current_source_dir(),
        'service_name': service_name
    })
    configure_file(
        input: 'org.mock.Speech.Provider.service.in',
        output: 'org.@0@.Speech.Provider.service'.format(service_name),
        configuration: c)
endforeach
