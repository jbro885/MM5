from m5 import *
from Device import PioDevice

class BadDevice(PioDevice):
    type = 'BadDevice'
    devicename = Param.String("Name of device to error on")
