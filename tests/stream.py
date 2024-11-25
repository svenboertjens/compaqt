from test_values import test_values
import compaqt as cq
#from compaqt import __compaqt_Py as cq

print('Testing streaming serialization')

# Filename for testing
f = 'test_stream.bin'

# Test 1 (regular use)

enc = cq.StreamEncoder(f, list)
enc.write(test_values)

cq.validate(file_name=f, err_on_invalid=True)
dec = cq.StreamDecoder(f)
if dec.read() != test_values:
    print(f"Invalid decoding (1)")

# Test 2 (preserve file)

enc = cq.StreamEncoder(f, list, preserve_file=True)
enc.write(test_values)

offset = enc.start_offset

cq.validate(file_name=f, err_on_invalid=True, file_offset=offset)

dec = cq.StreamDecoder(f, file_offset=offset)
if dec.read() != test_values:
    print(f"Invalid decoding (2)")

# Test 3 (file offset)

offset = 17

enc = cq.StreamEncoder(f, list, file_offset=offset)
enc.write(test_values)

cq.validate(file_name=f, err_on_invalid=True, file_offset=offset)

dec = cq.StreamDecoder(f, file_offset=offset)
if dec.read() != test_values:
    print(f"Invalid decoding (3)")

# Test 4 (incremental reading/writing)

enc = cq.StreamEncoder(f, list)
enc.write(test_values)
enc.write(test_values)

cq.validate(file_name=f, err_on_invalid=True)

dec = cq.StreamDecoder(f)
if dec.read(len(test_values)) != test_values:
    print(f"Invalid decoding (4.1)")
if dec.read(len(test_values)) != test_values:
    print(f"Invalid decoding (4.2)")

# Clean up file
import os
os.remove(f)

print('Finished\n')

