from m5 import *
class DiskImage(SimObject):
    type = 'DiskImage'
    abstract = True
    image_file = Param.String("disk image file")
    read_only = Param.Bool(False, "read only image")

class RawDiskImage(DiskImage):
    type = 'RawDiskImage'

class CowDiskImage(DiskImage):
    type = 'CowDiskImage'
    child = Param.DiskImage("child image")
    table_size = Param.Int(65536, "initial table size")
    image_file = ''
