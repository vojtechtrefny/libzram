A simple library for [zRAM](https://www.kernel.org/doc/Documentation/blockdev/zram.txt) management. Originally part of the now deprecated [libblockdev](https://github.com/storaged-project/libblockdev) KBD plugin.

### Building

```
meson setup builddir && cd builddir
meson compile
```

#### Dependencies
 * Fedora
   * `libblockdev-utils-devel`
   * `glib2-devel`
   * `gobject-introspection-devel`
 * Ubuntu/Debian
   * `libblockdev-utils-dev`
   * `libglib2.0-dev`
   * `libgirepository1.0-dev`
