config = configuration_data()
config.set('provider_iface', iface_xml.full_path())

configure_file(
  input: 'mock_speech_provider.py.in',
  output: 'mock_speech_provider.py',
  configuration: config
)

service_names = ['mock', 'mock2', 'mock3']
foreach service_name : service_names
    c = configuration_data({
        'service_exec_dir': meson.current_build_dir(),
        'service_name': service_name
    })
    configure_file(
        input: 'org.mock.Speech.Provider.service.in',
        output: 'org.@0@.Speech.Provider.service'.format(service_name),
        configuration: c)
endforeach
