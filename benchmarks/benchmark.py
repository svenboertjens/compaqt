import compaqt
import timeit

iterations = 1000_0000

def benchmark(value):
    encoded = compaqt.encode(value)
    
    encode = timeit.timeit(lambda: compaqt.encode(value), number=iterations)
    decode = timeit.timeit(lambda: compaqt.decode(encoded), number=iterations)
    
    print(f"\nType '{type(value).__name__}': {value}\nEncode: {encode:.6f} s\nDecode: {decode:.6f} s\nSize:   {len(encoded)} bytes")

print(f"Iterations: {iterations}")

benchmark(['Hello', 'world!', 'test', 'value', '123', '456'])
benchmark({1: 'item', 'key': 'val', b'C': 'Python'})

