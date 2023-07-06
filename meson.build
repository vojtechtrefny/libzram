project('libzram',
        'c',
        version : '0.1')

zram_description = 'Library for zRAM management'

glib_dep = dependency('glib-2.0')
bd_dep = dependency('blockdev-utils')

zram_headers = ['include/libzram.h']
zram_sources = ['src/libzram.c']

inc = include_directories('include')
install_headers('include/libzram.h')

zram_lib = library('zram',
                   zram_sources,
                   include_directories : inc,
                   dependencies : [glib_dep, bd_dep],
                   install : true)

pkg_mod = import('pkgconfig')
pkg_mod.generate(name : meson.project_name(),
                 filebase : 'zram',
                 description : zram_description,
                 subdirs : meson.project_name(),
                 libraries : zram_lib,
                 requires : 'glib-2.0 blockdev-utils')
