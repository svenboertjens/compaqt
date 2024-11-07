from test_values import test_values
import compaqt as cq
#from compaqt import __compaqt_Py as cq

print('Testing regular serialization')

def shorten(v: any) -> str:
    s = str(v)
    
    if len(s) > 64:
        s = s[:64] + ' ... (cut off)'
    
    return s

def test(v: any) -> None:
    try:
        encoded = cq.encode(v)
        
        if v != cq.decode(encoded):
            print(f'Failed: {shorten(v)}\n')
        
        if cq.validate(encoded, err_on_invalid=False) == False:
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
cq.encode(test_values, file_name=f)

# Validate the file
if cq.validate(file_name=f) == False:
    print(f"Incorrectly invalidated file '{f}'\n")

# Read the file again
if cq.decode(file_name=f) != test_values:
    print(f"Incorrectly decoded file '{f}'\n")

# Clean up file
import os
os.remove(f)

print('Finished\n')

