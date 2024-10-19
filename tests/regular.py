from test_values import test_values
from compaqt import encode, decode, validate

def shorten(v: any) -> str:
    s = str(v)
    
    if len(s) > 64:
        s = s[:64] + ' ... (cut off)'
    
    return s

def test(v: any) -> None:
    try:
        encoded = encode(v)
        
        if v != decode(encoded):
            print(f'Failed: {shorten(v)}\n')
        
        if validate(encoded, err_on_invalid=False) == False:
            print(f'Incorrectly invalidated: {shorten(v)}\n')
        
    except Exception as e:
        print(f'Error: {e}\nFor value: {shorten(v)}\n')

# Test all values regularly
for v in test_values:
    test(v)

# Test the entire list
test(test_values)

# Write the entire list to a file
f = 'test_regular.bin'
encode(test_values, filename=f)

# Validate the file
if validate(filename=f) == False:
    print(f"Incorrectly invalidated file '{f}'\n")

# Read the file again
if decode(filename=f) != test_values:
    print(f"Incorrectly decoded file '{f}'\n")

# Clean up file
import os
os.remove(f)

