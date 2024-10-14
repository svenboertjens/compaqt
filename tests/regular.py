from test_values import test_values
from compaqt import encode, decode

def shorten(v: any) -> str:
    s = str(v)
    
    if len(s) > 64:
        s = s[:64] + ' ... (cut off)'
    
    return s

def test(v: any) -> None:
    try:
        if v != decode(encode(v)):
            print(f'Failed: {shorten(v)}\n')
        
    except Exception as e:
        print(f'Error: {e}\nFor value: {shorten(v)}\n')

for v in test_values:
    test(v)

test(test_values)

