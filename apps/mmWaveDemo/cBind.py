from ctypes import cdll
serial = cdll.LoadLibrary('./serial.so')

class serial(object):
    def __init__(self):
        self.obj = serial.initialize()

    def read(self):
        serial.read(self.obj)