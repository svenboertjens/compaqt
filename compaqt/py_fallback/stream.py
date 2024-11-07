from .custom_types import CustomWriteTypes, CustomReadTypes
from .exceptions import *
from .serialize import encode_item, decode_item
from .metadata import rd_metadata
from .decode import DecodeData
from .masks import *

class StreamData:
    def __init__(
            self,
            filename,
            file_offset
        ):
        
        self.filename = filename
        self.num_items = 0
        self.tp = None
        self.file_offset = self.total_offset = file_offset

class StreamEncoder:
    def __init__(
            self,
            file_name: str,
            value_type: type=list,
            _=None,
            custom_types: CustomWriteTypes=None,
            resume_stream: bool=False,
            file_offset: int=0,
            preserve_file: bool=False
        ):
        
        if type(file_name) != str:
            raise ValueError(f"File name argument must be of type 'str', got '{type(file_name).__name__}'")
        
        if value_type != list and value_type != dict:
            raise ValueError(f"Streaming mode only supports initialization with list/dict types, got '{value_type.__name__}'")
        
        if custom_types != None and type(custom_types) != CustomWriteTypes:
            raise ValueError(f"The custom types object must be of type 'CustomWriteTypes', got '{type(custom_types).__name__}'")
        
        if file_offset < 0:
            raise ValueError(f"File offset must be positive, got {file_offset}")
        
        self._data = StreamData(file_name, file_offset)
        self._custom_types = custom_types
        
        if resume_stream:
            file = open(file_name, "rb")
            file.seek(file_offset)
            
            buf = file.read(9)
            buf0 = buf[0]
            dtchar = buf0 & 0b111
            
            if buf0 & 0b11111000 != 0b11111000 or (dtchar != DT_ARRAY and dtchar != DT_DICTN):
                raise ValueError("The existing file data does not match the encoding stream expectations")
            
            self._data.tp = dtchar == DT_ARRAY and list or dict
            self._data.file_offset = file_offset
            self._data.num_items = int.from_bytes(buf[1:], 'little', signed=False)
            
        else:
            self._data.tp = value_type
            
            if preserve_file:
                file = open(file_name, "ab")
                self._data.file_offset = file.tell()
                
            else:
                file = open(file_name, "wb")
                file.seek(file_offset)
            
            if value_type == list:
                file.write(bytes([DT_ARRAY | 0b11111000]))
            else:
                file.write(bytes([DT_DICTN | 0b11111000]))
            
            file.write(bytes([0] * 8))
        
        self.start_offset = self.total_offset = self._data.file_offset
        
        file.close()
    
    def write(
            self,
            value: any,
            _=None,
            __=None
        ):
        
        if self._data == None:
            raise ValueError("Received an invalid StreamEncoder object")
        
        if type(value) != self._data.tp:
            raise ValueError(f"Streaming mode requires values to continue as the same type. Started with type '{self._data.tp.__name__}', got '{type(value).__name__}'")
        
        self._data.msg = bytearray()
        file = open(self._data.filename, "ab")
        file.seek(self._data.file_offset)
        
        self._data.num_items += len(value)
        
        if self._data.tp == list:
            for item in value:
                encode_item(item, file.write, self._custom_types)
        
        else:
            for key, val in value.items():
                encode_item(key, file.write, self._custom_types)
                encode_item(val, file.write, self._custom_types)
        
        self._data.total_offset = file.tell()
        file.close()
        
        file = open(self._data.filename, "r+b")
        file.seek(self._data.file_offset + 1)
        file.write(self._data.num_items.to_bytes((self._data.num_items.bit_length() + 7) >> 3, "little", signed=False))
        file.close()
    
    def finalize(self):
        self._data = None
        self._custom_types = None

def decode_offset_check(data: DecodeData, length: int):
    if data.offset + length > data.max:
        data.msg = memoryview(data.file.read(data.max))
        data.max = len(data.msg)
        
        if data.max < length:
            raise DecodingError("Likely received an invalid or corrupted bytes object")
        
        data.offset = 0

class StreamDecoder:
    def __init__(
            self,
            file_name: str,
            chunk_size=1024*256,
            custom_types: CustomReadTypes=None,
            file_offset: int=0
        ):
        
        if type(file_name) != str:
            raise ValueError(f"File name argument must be of type 'str', got '{type(file_name).__name__}'")
        
        if custom_types != None and type(custom_types) != CustomReadTypes:
            raise ValueError(f"The custom types object must be of type 'CustomReadTypes', got '{type(custom_types).__name__}'")
        
        if file_offset < 0:
            raise ValueError(f"File offset must be positive, got {file_offset}")
        
        self._data = StreamData(file_name, file_offset)
        self._data.chunk_size = chunk_size
        
        self._custom_types = custom_types
        
        if chunk_size < 9:
            raise ValueError(f"Chunk size argument must be more than 8, got {chunk_size}")
        
        file = open(file_name, "rb")
        file.seek(file_offset)
        
        buf = file.read(9)
        buf0 = buf[0]
        dtchar = buf0 & 0b111
        
        if dtchar == DT_ARRAY:
            self._data.tp = list
        elif dtchar == DT_DICTN:
            self._data.tp = dict
        else:
            raise ValueError("Encoded data must start with a list or dict object for stream objects")
        
        data = DecodeData(memoryview(buf), lambda _, __: None, None)
        
        self._data.num_items = rd_metadata(data)
        self._data.file_offset += data.offset
        
        self.items_remaining = self._data.num_items
        self.total_offset = self._data.file_offset
        
        file.close()
    
    def read(
            self,
            num_items: int=None,
            _=None,
            chunk_size=None
        ):
        
        if self._data == None:
            raise ValueError("Received an invalid StreamEncoder object")
        
        if chunk_size != None:
            if chunk_size < 9:
                raise ValueError(f"Chunk size argument must be more than 8, got {chunk_size}")
            
            self._data.chunk_size = chunk_size
        
        if num_items == None or num_items > self._data.num_items:
            num_items = self._data.num_items
            self._data.num_items = 0
        elif num_items < 1:
            raise ValueError(f"Num items argument must be at least 1, got {num_items}")
        else:
            self._data.num_items -= num_items
        
        file = open(self._data.filename, "rb")
        file.seek(self._data.file_offset)
        
        data = DecodeData(memoryview(file.read(self._data.chunk_size)), decode_offset_check, file)
        value = self._data.tp()
        
        if value == []:
            for _ in range(num_items):
                value.append(decode_item(data, self._custom_types))
        
        else:
            for _ in range(num_items):
                value[decode_item(data, self._custom_types)] = decode_item(data, self._custom_types)
        
        self._data.file_offset = file.tell() - data.max + data.offset
        file.close()
        
        self.items_remaining = self._data.num_items
        self.total_offset = self._data.file_offset
        
        return value
    
    def finalize(self):
        self._data = None
        self._custom_types = None

