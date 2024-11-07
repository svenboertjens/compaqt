from .exceptions import EncodingError, DecodingError
from .metadata import rd_metadata_lm2
from .decode import DecodeData
from .masks import *

def range_check(idx):
    if type(idx) != int:
        raise ValueError(f"Expected the value on index 0 to be of type 'int', got '{type(idx).__name__}'")
    elif idx > 31:
        raise ValueError(f"Custom type index out of range: got {idx}, max is 31")
    elif idx < 0:
        raise ValueError(f"Custom type index should be positive, got {idx}")

class CustomWriteTypes:
    def __init__(self, custom_types: dict):
        self.types = {}
        
        for idx, (tp, func) in custom_types.items():
            if type(tp) != type:
                raise ValueError(f"Expected a key of type 'type', got '{type(tp).__name__}'")
            
            range_check(idx)
            
            if not callable(func):
                raise ValueError(f"Expected values on index 1 to be of type 'function' (or other callables), got '{type(func).__name__}'")
            
            self.types[tp] = (idx << 3, func)

class CustomReadTypes:
    def __init__(self, custom_types: tuple):
        self.types = [None] * 32
        
        for idx, func in custom_types.items():
            if not callable(func):
                raise ValueError(f"Expected values of type 'function' (or other callables), got '{type(func).__name__}'")
            
            range_check(idx)
            
            self.types[idx] = func

def custom_encode(value: any, append, types: CustomWriteTypes):
    obj = types.types.get(type(value))
        
    if obj == None:
        raise EncodingError(f"Received an invalid datatype: '{type(value).__name__}'")
    
    b = obj[1](value)
    
    if type(b) != bytes:
        raise ValueError(f"Expected a 'bytes' object from a custom type write function, got '{type(b).__name__}'")
    
    append(bytes([DT_EXTND | obj[0]]))
    
    if b == b"":
        msg.append(0)
    else:
        l = len(b)
        num_bytes = (l.bit_length() + 7) >> 3
        
        append(bytes([num_bytes]))
        append(l.to_bytes(num_bytes, 'little', signed=False))
        
        append(b)

def custom_decode(data: DecodeData, types: CustomReadTypes):
    if types == None:
        raise DecodingError("Likely received an invalid or corrupted bytes object")
    
    data.check(data, 2)
    
    idx = data.msg[data.offset] >> 3
    func = types.types[idx]
    
    if not func:
        raise DecodingError(f"Could not find a valid function on ID {idx}. Did you use the same custom type IDs as when encoding?")
    
    num_bytes = data.msg[data.offset + 1]
    data.offset += 2
    
    length = rd_metadata_lm2(data, num_bytes)
    
    offset = data.offset
    data.offset += length
    
    return func(data.msg[offset:data.offset].tobytes())

# Pretty names for the classes
encoder_types = CustomWriteTypes
decoder_types = CustomReadTypes

