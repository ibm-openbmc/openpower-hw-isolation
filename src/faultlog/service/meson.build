systemd_system_unit_dir = systemd_dep.get_variable(
    pkgconfig:'systemdsystemunitdir')
conf_data = configuration_data()
conf_data.set('bindir', get_option('prefix') / get_option('bindir'))

input_files = ['faultlog_hostpoweron.service.in', 'faultlog_periodic.timer']

output_files = ['faultlog_hostpoweron.service', 'faultlog_periodic.timer']

counter = 0
foreach i : input_files
  # Get the index of the current iteration

  # Configure the files using configure_file()
  configure_file(
    input : i,
    output : output_files[counter],
    configuration: conf_data,
    install: true,
    install_dir: systemd_system_unit_dir
  )
  counter += 1
endforeach
    
systemd_alias = [[
    '../faultlog_hostpoweron.service', 'obmc-host-startmin@0.target.wants/faultlog_hostpoweron.service'
]]

systemd_alias += [[
    '../faultlog_periodic.timer', 'timers.target.wants/faultlog_periodic.timer'
]]

foreach service: systemd_alias
    # Meson 0.61 will support this:
    #install_symlink(
    #      service,
    #      install_dir: systemd_system_unit_dir,
    #      pointing_to: link,
    #  )
    meson.add_install_script(
        'sh', '-c',
        'mkdir -p $(dirname $DESTDIR/@0@/@1@)'.format(systemd_system_unit_dir,
            service[1]),
    )   
    meson.add_install_script(
        'sh', '-c',
        'ln -s @0@ $DESTDIR/@1@/@2@'.format(service[0], systemd_system_unit_dir,
            service[1]),
    )   
endforeach

