A simple library for [zRAM](https://www.kernel.org/doc/Documentation/blockdev/zram.txt) management. Originally part of the now deprecated [libblockdev](https://github.com/storaged-project/libblockdev) KBD plugin.

### Building

```
meson setup builddir && cd builddir
meson compile
```

#### Dependencies
 * `libblockdev-utils`
 * `glib2`
