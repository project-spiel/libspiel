conf = configuration_data()
conf.set('service_exec_dir', join_paths(meson.current_source_dir()))

service_names = ['Mock', 'Mock2', 'Mock3']

foreach service_name : service_names
    c = configuration_data({
        'service_exec_dir': meson.current_source_dir(),
        'service_name': service_name
    })
    configure_file(
        input: 'org.freedesktop.Speech.Synthesis.Mock.service.in',
        output: 'org.freedesktop.Speech.Synthesis.@0@.service'.format(service_name),
        configuration: c)
endforeach
