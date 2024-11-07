from .custom_types import CustomWriteTypes, CustomReadTypes, custom_encode, custom_decode
from .decode import DecodeData
from .exceptions import *
from .metadata import *
from .masks import *
import struct

def encode_bytes(value: bytes, append):
    wr_metadata(append, DT_BYTES, len(value))
    append(value)

def encode_str(value: str, append):
    encoded = value.encode()
    wr_metadata(append, DT_STRNG, len(encoded))
    append(encoded)

def encode_int(value: int, append):
    num_bytes = (value.bit_length() + 8) >> 3
    wr_metadata(append, DT_INTGR, num_bytes)
    append(value.to_bytes(num_bytes, 'little', signed=True))

def encode_float(value: float, append):
    append(bytes([DT_FLOAT]) + struct.pack('<d', value))

def encode_bool(value: bool, append):
    append(bytearray([value == True and DT_BOOLT or DT_BOOLF]))

def encode_none(value: type(None), append):
    append(bytearray([DT_NONTP]))

def encode_list(value: list, append):
    wr_metadata(append, DT_ARRAY, len(value))
    
    for item in value:
        encode_item(item, append, None)

def encode_dict(value: dict, append):
    wr_metadata(append, DT_DICTN, len(value))
    
    for key, val in value.items():
        encode_item(key, append, None)
        encode_item(val, append, None)

encode_type_lookup = {
    bytes: encode_bytes,
    str: encode_str,
    int: encode_int,
    float: encode_float,
    bool: encode_bool,
    type(None): encode_none,
    list: encode_list,
    dict: encode_dict
}

def encode_item(value: any, append, custom_types: CustomWriteTypes):
    encode_func = encode_type_lookup.get(type(value))
    
    if encode_func:
        encode_func(value, append)
    else:
        custom_encode(value, append, custom_types)

def decode_bytes(data: DecodeData, _):
    length = rd_metadata(data)
    
    offset = data.offset
    data.offset += length
    
    return data.msg[offset:data.offset].tobytes()

def decode_string(data: DecodeData, _):
    length = rd_metadata(data)
    
    offset = data.offset
    data.offset += length
    
    return data.msg[offset:data.offset].tobytes().decode()

def decode_integer(data: DecodeData, _):
    length = rd_metadata(data)
    
    offset = data.offset
    data.offset += length
    
    return int.from_bytes(data.msg[offset:data.offset], 'little', signed=True)

def decode_float(data: DecodeData):
    data.check(data, 8)
    
    offset = data.offset
    data.offset += 8
    
    return struct.unpack('<d', data.msg[offset:data.offset])[0]

def decode_list(data: DecodeData, _):
    num_items = rd_metadata(data)
    return [decode_item(data, None) for _ in range(num_items)]

def decode_dict(data: DecodeData, _):
    num_items = rd_metadata(data)
    return {decode_item(data, None): decode_item(data, None) for _ in range(num_items)}

decode_group_lookup = {
    DT_FLOAT: decode_float,
    DT_BOOLT: lambda x: True,
    DT_BOOLF: lambda x: False,
    DT_NONTP: lambda x: None
}

def decode_group(data: DecodeData, _):
    decode_func = decode_group_lookup.get(data.msg[data.offset])
    
    data.check(data, 1)
    data.offset += 1
    
    if decode_func:
        return decode_func(data)
    else:
        raise DecodingError("Likely received an invalid bytes object")

decode_type_lookup = {
    DT_BYTES: decode_bytes,
    DT_STRNG: decode_string,
    DT_INTGR: decode_integer,
    DT_GROUP: decode_group,
    DT_ARRAY: decode_list,
    DT_DICTN: decode_dict,
    DT_EXTND: custom_decode
}

def decode_item(data: DecodeData, custom_types: CustomReadTypes):
    data.check(data, 1)
    return decode_type_lookup.get(data.msg[data.offset] & 0b111)(data, custom_types)

