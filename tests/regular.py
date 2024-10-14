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
            
            if validate(encoded, err_on_invalid=False) == True:
                print(f'Incorrectly validated: {shorten(v)}\n')
        
        elif validate(encoded, err_on_invalid=False) == False:
            print(f'Incorrectly invalidated: {shorten(v)}\n')
        
    except Exception as e:
        print(f'Error: {e}\nFor value: {shorten(v)}\n')

for v in test_values:
    test(v)

test(test_values)

