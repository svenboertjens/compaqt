import compaqt as cq
#from compaqt import __compaqt_Py as cq

print('Testing custom types serialization')

class StringCustom:
    def __init__(self, value):
        self.value = value

def string_wr(value: StringCustom) -> bytes:
    return value.value.encode()

def string_rd(value: bytes) -> StringCustom:
    return StringCustom(value.decode())

class IntegerCustom:
    def __init__(self, value):
        self.value = value

def int_wr(value: IntegerCustom) -> bytes:
    # Serialize the integer to bytes
    return value.value.to_bytes(4, byteorder='big', signed=True)

def int_rd(value: bytes) -> IntegerCustom:
    # Deserialize bytes back to an integer
    return IntegerCustom(int.from_bytes(value, byteorder='big', signed=True))

class FloatCustom:
    def __init__(self, value):
        self.value = value

def float_wr(value: FloatCustom) -> bytes:
    # Convert the float to bytes
    return str(value.value).encode()

def float_rd(value: bytes) -> FloatCustom:
    # Convert bytes back to a float
    return FloatCustom(float(value.decode()))

# Set up encoder and decoder mappings
enc = cq.types.encoder_types({
    0: (StringCustom, string_wr),
    1: (IntegerCustom, int_wr),
    2: (FloatCustom, float_wr)
})

dec = cq.types.decoder_types({
    0: string_rd,
    1: int_rd,
    2: float_rd
})

# Test cases

def test_custom_type(obj, expected_value):
    obj = obj(expected_value)
    
    encoded = cq.encode(obj, custom_types=enc)
    decoded = cq.decode(encoded, custom_types=dec)

    if decoded.value != expected_value:
        print(f"Failed for type {type(obj).__name__}")

# Run tests for each custom type
test_custom_type(StringCustom, "test")
test_custom_type(IntegerCustom, 12345)
test_custom_type(FloatCustom, 123.456)

# Additional tests

# Test edge cases and invalid inputs
try:
    # Unsupported type
    cq.encode(2j + 3, custom_types=enc)
    print("Failed: Unsupported type test")
except:
    pass

try:
    # Encoding a custom type without corresponding decode function
    enc_missing_decode = cq.types.encoder_types({3: (IntegerCustom, int_wr)})
    encoded = cq.encode(IntegerCustom(6789), custom_types=enc_missing_decode)
    cq.decode(encoded, custom_types=dec)  # This should fail
    print("Failed: Missing decode function test")
except:
    pass

try:
    # Decoding with an incorrect type ID
    encoded = cq.encode(StringCustom("incorrect test"), custom_types=enc)
    # Remove StringCustom's decoder function
    dec_missing = cq.types.decoder_types({1: int_rd, 2: float_rd})
    cq.decode(encoded, custom_types=dec_missing)
    print("Failed: Incorrect type ID test")
except:
    pass

print('Finished\n')

