from .decode import DecodeData

def wr_metadata_lm2_mask(append, mask: int, num_bytes: int):
    append(bytes([mask | 0b00011000 | ((num_bytes - 1) << 5)]))

def wr_metadata_lm2(append, length: int, num_bytes: int):
    append(length.to_bytes(num_bytes, 'little', signed=False))

def wr_metadata(append, mask: int, length: int):
    if length < 16:
        append(bytes([mask | (length << 4)]))
    elif length < 2048:
        append(bytes([mask | 0b00001000 | ((length << 5) & 0xFF), (length >> 3) & 0xFF]))
    else:
        num_bytes = (length.bit_length() + 7) >> 3
        wr_metadata_lm2_mask(append, mask, num_bytes)
        wr_metadata_lm2(append, length, num_bytes)

def rd_metadata_lm2(data: DecodeData, num_bytes: int):
    offset = data.offset
    data.offset += num_bytes
    
    length = int.from_bytes(data.msg[offset:data.offset], 'little', signed=False)
    
    data.check(data, length)
    return length

def rd_metadata(data: DecodeData):
    byte = data.msg[data.offset]
    mode = byte & 0b11000
    
    if mode == 0b00000 or mode == 0b10000:
        data.check(data, 1)
        
        length = byte >> 4
        data.offset += 1
        
        data.check(data, length)
        return length
    elif mode == 0b01000:
        data.check(data, 2)
        
        length = (byte >> 5) | (data.msg[data.offset + 1] << 3)
        data.offset += 2
        
        data.check(data, length)
        return length
    else:
        data.check(data, 1)
        
        num_bytes = (byte >> 5) + 1
        data.offset += 1
        
        data.check(data, num_bytes)
        return rd_metadata_lm2(data, num_bytes)
