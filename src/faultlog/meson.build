cpp = meson.get_compiler('cpp')

sdbusplus_dep = dependency(
    'sdbusplus',
    fallback: ['sdbusplus', 'sdbusplus_dep'],
)

phosphor_dbus_interfaces_dep = dependency(
    'phosphor-dbus-interfaces',
    fallback: [
        'phosphor-dbus-interfaces',
        'phosphor_dbus_interfaces_dep'
    ],  
)

phosphor_logging_dep = dependency(
    'phosphor-logging',
    fallback: ['phosphor-logging', 'phosphor_logging_dep'],
)

if cpp.has_header('nlohmann/json.hpp')
    nlohmann_json = declare_dependency()
else
    nlohmann_json_proj = subproject('nlohmann', required: true)
    nlohmann_json = nlohmann_json_proj.get_variable('nlohmann_json_dep')
    nlohmann_json = nlohmann_json.as_system('system')
endif

systemd_dep = dependency('systemd')

faultlog_sources = [ 
        'fault_log_main.cpp',
        'faultlog_records.cpp',
        'servicable_records.cpp',
        'faultlog_policy.cpp'
        ]

faultlog_dependencies = [ 
        format,
        phosphor_dbus_interfaces,
        phosphor_logging,
        sdbusplus,
        nlohmann_json
    ]   
executable('faultlog',
           faultlog_sources,
           dependencies: faultlog_dependencies,
           install : true
          )

