cpp = meson.get_compiler('cpp')

sdbusplus_dep = dependency('sdbusplus', fallback: ['sdbusplus', 'sdbusplus_dep'])

phosphor_dbus_interfaces_dep = dependency(
    'phosphor-dbus-interfaces',
    fallback: ['phosphor-dbus-interfaces', 'phosphor_dbus_interfaces_dep'],
)
sdeventplus = dependency('sdeventplus')
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
libphal = cpp.find_library('phal')


systemd_dep = dependency('systemd')

faultlog_sources = [
    'faultlog_main.cpp',
    'guard_with_eid_records.cpp',
    'util.cpp',
    'guard_without_eid_records.cpp',
    'faultlog_policy.cpp',
    'unresolved_pels.cpp',
    'deconfig_records.cpp',
    'deconfig_reason.cpp',
    'poweron_time.cpp',
    'service_alerts_time.cpp',
]

faultlog_dependencies = [
    phosphor_dbus_interfaces,
    phosphor_logging,
    sdbusplus,
    nlohmann_json,
    libpdbg,
    libdtapi,
    libguard,
    libphal,
    sdeventplus,
]

executable(
    'faultlog',
    faultlog_sources,
    dependencies: faultlog_dependencies,
    include_directories: include_directories('../../'),
    install: true,
)

faultlog_poweron_time_sources = [
    'faultlog_poweron_time.cpp',
    'poweron_time.cpp',
]

faultlog_poweron_time_dependencies = [phosphor_logging, sdbusplus]

executable(
    'faultlog-poweron-time',
    faultlog_poweron_time_sources,
    dependencies: faultlog_poweron_time_dependencies,
    include_directories: include_directories('../../'),
    install: true,
)

faultlog_service_alerts_monitor_sources = [
    'faultlog_service_alerts_monitor.cpp',
    'service_alerts_time.cpp',
    'util.cpp',
]

faultlog_service_alerts_monitor_dependencies = [
    phosphor_logging,
    sdbusplus,
    sdeventplus,
    libguard,
]

executable(
    'faultlog-service-alerts-monitor',
    faultlog_service_alerts_monitor_sources,
    dependencies: faultlog_service_alerts_monitor_dependencies,
    include_directories: include_directories('../../'),
    install: true,
)

subdir('service')
