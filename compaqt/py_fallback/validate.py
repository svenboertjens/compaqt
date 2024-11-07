from .exceptions import ValidationError
from .metadata import rd_metadata
from .decode import DecodeData
from .masks import *

def metadata_item(data: DecodeData):
    length = rd_metadata(data)
    data.offset += length

def group_item(data: DecodeData):
    func = group_lookup.get(data.msg[data.offset])
    
    if func:
        func(data)
    else:
        raise ValidationError("The bytes object does not appear valid GROUP")

def static_item(data: DecodeData, size: int):
    data.offset += size

group_lookup = {
    DT_FLOAT: lambda x: static_item(x, 9),
    DT_BOOLT: lambda x: static_item(x, 1),
    DT_BOOLF: lambda x: static_item(x, 1),
    DT_NONTP: lambda x: static_item(x, 1)
}

def list_item(data: DecodeData):
    num_items = rd_metadata(data)
    
    for _ in range(num_items):
        validate_item(data)

def dict_item(data: DecodeData):
    num_items = rd_metadata(data)
    
    for _ in range(num_items):
        validate_item(data)
        validate_item(data)

def validate_item(data: DecodeData):
    data.check(data, 0)
    
    func = type_lookup.get(data.msg[data.offset] & 0b111)
    
    if func:
        func(data)
    else:
        raise ValidationError("The bytes object does not appear valid ITEM")

type_lookup = {
    DT_BYTES: metadata_item,
    DT_STRNG: metadata_item,
    DT_INTGR: metadata_item,
    DT_GROUP: group_item,
    DT_ARRAY: list_item,
    DT_DICTN: dict_item,
    
    #DT_EXTND: decode_extend
}

def limit_check(data: DecodeData, length: int):
    if data.offset + length > data.max:
        raise ValidationError("The bytes object does not appear valid REG")

def chunk_check(data: DecodeData, length: int):
    if data.offset + length > data.max:
        data.file.seek(data.offset, 1)
        data.msg = memoryview(fdata.ile.read(data.max))
        
        data.offset = 0
        
        if len(data.msg) != data.max:
            raise ValidationError("The bytes object does not appear valid FILE")

def validate(encoded: bytes=None, file_name: str=None, file_offset: int=0, chunk_size: int=0, err_on_invalid: bool=False):
    if encoded != None:
        data = DecodeData(memoryview(encoded), limit_check)
        validate_item(data)
        valid = data.offset == data.max
        
        if err_on_invalid and valid == False:
            raise ValidationError("The bytes object does not appear valid END")

        return valid
    elif file_name != None:
        file = open(file_name, 'rb')
        file.seek(file_offset)
        
        if chunk_size < 1:
            chunk_size = -1
        
        data = DecodeData(file.read(chunk_size), chunk_check, file=file)
        validate_item(data)
        
        return True
    else:
        raise ValueError("Expected either the 'encoded' or the 'file_name' argument, got neither")

