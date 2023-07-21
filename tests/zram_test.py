#!/usr/nin/python3

import unittest
import os
import time
from contextlib import contextmanager

import gi
gi.require_version("Zram", "0.1")
gi.require_version("GLib", "2.0")

from gi.repository import Zram, GLib


def _can_load_zram():
    """Test if we can load the zram module"""

    if os.system("lsmod|grep zram >/dev/null") != 0:
        # not loaded
        return True
    elif os.system("rmmod zram") == 0:
        # successfully unloaded
        return True
    else:
        # loaded and failed to unload
        return False


@contextmanager
def _track_module_load(test_case, mod_name, loaded_attr):
    setattr(test_case, loaded_attr, os.system("lsmod|grep %s > /dev/null" % mod_name) == 0)
    try:
        yield
    finally:
        setattr(test_case, loaded_attr, os.system("lsmod|grep %s > /dev/null" % mod_name) == 0)


def read_file(filename):
    with open(filename, "r") as f:
        content = f.read()
    return content


class ZRAMTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        if os.geteuid() != 0:
            raise unittest.SkipTest("requires root privileges")

        if not _can_load_zram():
            raise unittest.SkipTest("cannot load the 'zram' module")

    def setUp(self):
        self.addCleanup(self._clean_up)
        self._loaded_zram_module = False

    def _clean_up(self):
        # make sure we unload the module if we loaded it
        if self._loaded_zram_module:
            os.system("rmmod zram")

    def test_create_destroy_devices(self):
        # the easiest case
        with _track_module_load(self, "zram", "_loaded_zram_module"):
            self.assertTrue(Zram.create_devices(2, [10 * 1024**2, 10 * 1024**2], [1, 2]))
            time.sleep(1)
            self.assertTrue(Zram.destroy_devices())
            time.sleep(1)

        # no nstreams specified
        with _track_module_load(self, "zram", "_loaded_zram_module"):
            self.assertTrue(Zram.create_devices(2, [10 * 1024**2, 10 * 1024**2], None))
            time.sleep(1)
            self.assertTrue(Zram.destroy_devices())
            time.sleep(1)

        # with module pre-loaded, but unsed
        self.assertEqual(os.system("modprobe zram num_devices=2"), 0)
        time.sleep(1)
        with _track_module_load(self, "zram", "_loaded_zram_module"):
            self.assertTrue(Zram.create_devices(2, [10 * 1024**2, 10 * 1024**2], [1, 1]))
            time.sleep(1)
            self.assertTrue(Zram.destroy_devices())
            time.sleep(1)

    def test_zram_add_remove_device(self):
        """Verify that it is possible to add and remove a zram device"""

        # the easiest case
        with _track_module_load(self, "zram", "_loaded_zram_module"):
            succ, device = Zram.add_device (10 * 1024**2, 4)
            self.assertTrue(succ)
            self.assertTrue(device.startswith("/dev/zram"))
            time.sleep(5)
            self.assertTrue(Zram.remove_device(device))

        # no nstreams specified
        with _track_module_load(self, "zram", "_loaded_zram_module"):
            succ, device = Zram.add_device (10 * 1024**2, 0)
            self.assertTrue(succ)
            self.assertTrue(device.startswith("/dev/zram"))
            time.sleep(5)
            self.assertTrue(Zram.remove_device(device))

        # create two devices
        with _track_module_load(self, "zram", "_loaded_zram_module"):
            succ, device = Zram.add_device (10 * 1024**2, 4)
            self.assertTrue(succ)
            self.assertTrue(device.startswith("/dev/zram"))

            succ, device2 = Zram.add_device (10 * 1024**2, 4)
            self.assertTrue(succ)
            self.assertTrue(device2.startswith("/dev/zram"))

            time.sleep(5)
            self.assertTrue(Zram.remove_device(device))
            self.assertTrue(Zram.remove_device(device2))

        # mixture of multiple devices and a single device
        with _track_module_load(self, "zram", "_loaded_zram_module"):
            self.assertTrue(Zram.create_devices(2, [10 * 1024**2, 10 * 1024**2], [1, 2]))
            time.sleep(5)
            succ, device = Zram.add_device (10 * 1024**2, 4)
            self.assertTrue(succ)
            self.assertTrue(device.startswith("/dev/zram"))
            time.sleep(5)
            self.assertTrue(Zram.destroy_devices())
            time.sleep(5)

    def test_zram_get_stats(self):
        """Verify that it is possible to get stats for a zram device"""

        with _track_module_load(self, "zram", "_loaded_zram_module"):
            self.assertTrue(Zram.create_devices(1, [10 * 1024**2], [2]))
            time.sleep(1)

        # XXX: this needs to get more complex/serious
        stats = Zram.get_stats("zram0")
        self.assertTrue(stats)

        # /dev/zram0 should work too
        stats = Zram.get_stats("/dev/zram0")
        self.assertTrue(stats)

        self.assertEqual(stats.disksize, 10 * 1024**2)
        # XXX: 'max_comp_streams' is currently broken on rawhide
        # https://bugzilla.redhat.com/show_bug.cgi?id=1352567
        # self.assertEqual(stats.max_comp_streams, 2)
        self.assertTrue(stats.comp_algorithm)

        # read 'num_reads' and 'num_writes' from '/sys/block/zram0/stat'
        sys_stats = read_file("/sys/block/zram0/stat").strip().split()
        self.assertGreaterEqual(len(sys_stats), 11)  # 15 stats since 4.19
        num_reads = int(sys_stats[0])
        num_writes = int(sys_stats[4])
        self.assertEqual(stats.num_reads, num_reads)
        self.assertEqual(stats.num_writes, num_writes)

        # read 'orig_data_size', 'compr_data_size', 'mem_used_total' and
        # 'zero_pages' from '/sys/block/zram0/mm_stat'
        sys_stats = read_file("/sys/block/zram0/mm_stat").strip().split()
        self.assertGreaterEqual(len(sys_stats), 7)  # since 4.18 we have 8 stats
        orig_data_size = int(sys_stats[0])
        compr_data_size = int(sys_stats[1])
        mem_used_total = int(sys_stats[2])
        zero_pages = int(sys_stats[5])
        self.assertEqual(stats.orig_data_size, orig_data_size)
        self.assertEqual(stats.compr_data_size, compr_data_size)
        self.assertEqual(stats.mem_used_total, mem_used_total)
        self.assertEqual(stats.zero_pages, zero_pages)

        # read 'invalid_io' and 'num_writes' from '/sys/block/zram0/io_stat'
        sys_stats = read_file("/sys/block/zram0/io_stat").strip().split()
        self.assertEqual(len(sys_stats), 4)
        invalid_io = int(sys_stats[2])
        self.assertEqual(stats.invalid_io, invalid_io)

        with _track_module_load(self, "zram", "_loaded_zram_module"):
            self.assertTrue(Zram.destroy_devices())


if __name__ == '__main__':
    unittest.main()
