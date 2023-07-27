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

libpdbg = meson.get_compiler('c').find_library('libpdbg')
libdtapi = dependency('libdt-api')
libguard = cpp.find_library('libguard')

systemd_dep = dependency('systemd')
sdeventplus = dependency('sdeventplus')

faultlog_sources = [ 
        'faultlog_main.cpp',
        'guard_with_eid_records.cpp',
        'util.cpp',
        'guard_without_eid_records.cpp',
        'faultlog_policy.cpp',
        'unresolved_pels.cpp',
        'deconfig_records.cpp',
        'deconfig_reason.cpp'
        ]

faultlog_dependencies = [ 
        format,
        phosphor_dbus_interfaces,
        phosphor_logging,
        sdbusplus,
        nlohmann_json,
        libpdbg,
        libdtapi,
        libguard,
        sdeventplus
    ]

executable('faultlog',
           faultlog_sources,
           dependencies: faultlog_dependencies,
           include_directories: include_directories('../../'),
           install : true
          )

subdir('service')
