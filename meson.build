project('libzram',
        'c',
        version : '0.1')

zram_description = 'Library for zRAM management'

glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')
bd_dep = dependency('blockdev-utils')

zram_headers = ['include/libzram.h']
zram_sources = ['src/libzram.c']

inc = include_directories('include')
install_headers('include/libzram.h')

zram_lib = shared_library('zram',
                          zram_sources,
                          include_directories : inc,
                          dependencies : [glib_dep, gio_dep, bd_dep],
                          install : true,
                          soversion : '0')

pkg_mod = import('pkgconfig')
pkg_mod.generate(name : meson.project_name(),
                 filebase : 'zram',
                 description : zram_description,
                 subdirs : meson.project_name(),
                 libraries : zram_lib,
                 requires : 'glib-2.0 blockdev-utils')

gnome = import('gnome')

zram_gir = gnome.generate_gir(zram_lib,
                              sources: zram_sources + zram_headers,
                              namespace: 'Zram',
                              nsversion: '0.1',
                              identifier_prefix: 'Zram',
                              symbol_prefix: 'zram',
                              export_packages: 'zram',
                              includes: [ 'GObject-2.0', 'Gio-2.0'],
                              header: 'include/libzram.h',
                              install: true)

tests_env = environment()
tests_env.set('GI_TYPELIB_PATH', meson.current_build_dir())
tests_env.set('LD_LIBRARY_PATH', meson.current_build_dir())
test('zram python test suite',
     find_program('./tests/zram_test.py'),
     workdir : meson.current_source_dir(),
     env : tests_env,
     timeout : 0)
