from test_values import test_values
from compaqt import StreamEncoder, StreamDecoder, validate

# Filename for testing
f = 'test_stream.bin'

# Test 1 (regular use)

enc = StreamEncoder(f, list)
enc.write(test_values)
enc.finalize()

validate(file_name=f, err_on_invalid=True)

dec = StreamDecoder(f)
if dec.read() != test_values:
    print(f"Invalid decoding (1)")
dec.finalize()

# Test 2 (preserve file)

enc = StreamEncoder(f, list, preserve_file=True)
enc.write(test_values)

offset = enc.start_offset

enc.finalize()

validate(file_name=f, err_on_invalid=True, file_offset=offset)

dec = StreamDecoder(f, file_offset=offset)
if dec.read() != test_values:
    print(f"Invalid decoding (2)")
dec.finalize()

# Test 3 (file offset)

offset = 17

enc = StreamEncoder(f, list, file_offset=offset)
enc.write(test_values)
enc.finalize()

validate(file_name=f, err_on_invalid=True, file_offset=offset)

dec = StreamDecoder(f, file_offset=offset)
if dec.read() != test_values:
    print(f"Invalid decoding (3)")
dec.finalize()

# Test 4 (incremental reading/writing)

enc = StreamEncoder(f, list)
enc.write(test_values)
enc.write(test_values)
enc.finalize()

validate(file_name=f, err_on_invalid=True)

dec = StreamDecoder(f)
if dec.read(len(test_values)) != test_values:
    print(f"Invalid decoding (4.1)")
if dec.read(len(test_values)) != test_values:
    print(f"Invalid decoding (4.2)")
dec.finalize()

# Clean up file
import os
os.remove(f)

