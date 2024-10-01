import compaqt
import timeit

iterations = 1000_000

def benchmark(value):
    encoded = compaqt.encode(value)
    
    encode = timeit.timeit(lambda: compaqt.encode(value), number=iterations)
    decode = timeit.timeit(lambda: compaqt.decode(encoded), number=iterations)
    
    print(f"Type '{type(value).__name__}': {value}\nEncode: {encode:.4f}\nDecode: {decode:.4f}\n")

print(f"Iterations: {iterations}\n")

benchmark(1024)
benchmark('Hello, world!')
benchmark(3.14)
benchmark(['Hello', 'world!'])
benchmark({1: b'item'})

