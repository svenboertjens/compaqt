from .custom_types import CustomWriteTypes, CustomReadTypes
from .serialize import encode_item, decode_item
from .exceptions import DecodingError
from .decode import DecodeData
from .metadata import *
from .masks import *

def encode(value: any, file_name: str=None, stream_compatible: bool=False, custom_types: CustomWriteTypes=None):
    msg = bytearray()
    
    if isinstance(value, list):
        num_items = len(value)
        
        if stream_compatible:
            wr_metadata_lm2_mask(msg.extend, DT_ARRAY, 8)
            wr_metadata_lm2(msg.extend, num_items, 8)
        else:
            wr_metadata(msg.extend, DT_ARRAY, num_items)
        
        for item in value:
            encode_item(item, msg.extend, custom_types)
        
    elif isinstance(value, dict):
        num_items = len(value)
        
        if stream_compatible:
            wr_metadata_lm2_mask(msg.extend, DT_DICTN, 8)
            wr_metadata_lm2(msg.extend, num_items, 8)
        else:
            wr_metadata(msg.extend, DT_DICTN, num_items)
        
        for key, val in value.items():
            encode_item(key, msg.extend, custom_types)
            encode_item(val, msg.extend, custom_types)
        
    else:
        encode_item(value, msg.extend, custom_types)
    
    if file_name:
        open(file_name, 'wb').write(msg)
        return None
    
    return bytes(msg)

def overread_check_regular(data, length: int):
    if data.offset + length > data.max:
        raise DecodingError("Likely received an invalid or corrupted bytes object")

def decode(encoded: bytes=None, file_name: str=None, custom_types: CustomReadTypes=None):
    if encoded != None:
        return decode_item(DecodeData(memoryview(encoded), overread_check_regular), custom_types)
    elif file_name != None:
        return decode_item(DecodeData(memoryview(open(file_name, 'rb').read()), overread_check_regular), custom_types)
    
    raise ValueError("Expected either the 'value' or 'file_name' argument, got neither")

