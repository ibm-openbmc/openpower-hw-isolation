# SPDX-License-Identifier: Apache-2.0

project('hardware_isolation', 'cpp', 'c',
    default_options: [
      'buildtype=debugoptimized',
      'cpp_std=c++20',
      'werror=true',
    ],
    meson_version: '>=0.58.0',
    version: '1.0.0',
)

# openbmc dependency packages list
format = dependency('fmt')
libpdbg = meson.get_compiler('c').find_library('libpdbg')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')
sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')

# Configuration header file(config.h) generation

conf_data = configuration_data()

conf_data.set_quoted('HW_ISOLATION_BUSNAME', get_option('HW_ISOLATION_BUSNAME'),
                      description : 'The Dbus busname to own for hardware isolation'
                    )

conf_data.set_quoted('HW_ISOLATION_OBJPATH', get_option('HW_ISOLATION_OBJPATH'),
                      description : 'The hardware isolation dbus root'
                    )

conf_data.set_quoted('PHAL_DEVTREE', get_option('PHAL_DEVTREE'),
                     description : 'The PHAL CEC device tree to get hardware details'
                    )

configure_file(configuration : conf_data,
               output : 'config.h'
              )

hardware_isolation_sources = [
        'hardware_isolation_entry.cpp',
        'hardware_isolation_manager_main.cpp',
        'phal_devtree_utils.cpp',
        'utils.cpp',
    ]

hardware_isolation_dependencies = [
        format,
        libpdbg,
        phosphor_dbus_interfaces,
        phosphor_logging,
        sdbusplus,
        sdeventplus
    ]

executable('openpower-hw-isolation',
           hardware_isolation_sources,
           dependencies: hardware_isolation_dependencies,
           install : true
          )

systemd_system_unit_dir = dependency('systemd').get_variable(
    pkgconfig: 'systemdsystemunitdir')

hw_isolation_service_unit = 'org.open_power.HardwareIsolation.service'
configure_file(
    configuration: {
        'BUSNAME' : get_option('HW_ISOLATION_BUSNAME'),
    },
    input: hw_isolation_service_unit,
    install : true,
    install_dir: systemd_system_unit_dir,
    output: hw_isolation_service_unit
    )
