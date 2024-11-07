# This file contains a class for decoding data

class DecodeData:
    def __init__(self, msg: memoryview, overread_check, file=None):
        self.offset = 0
        self.msg = msg
        self.max = len(msg)
        self.file = file
        self.check = overread_check

