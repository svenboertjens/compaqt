import compaqt
import msgpack
import pickle
import json

import timeit

iterations = 1000_000

values = [
    1024,
    'Hello, world!',
    3.142,
    ['hello', 'compaqt!'],
    {'17': 'dictionary'},
    {
        'Word': 'by word',
        'this': 'dict is',
        'written': ',',
        'with': 'items',
        'such': 'as',
        'numbers': 2008,
        'bools': True,
        'and': 'last but',
        'not': 'least',
        'strings': '"Hello, world!"',
    },
]

def compare(name, encode, decode):
    total = 0
    
    for v in values:
        total += timeit.timeit(lambda: decode(encode(v)), number=iterations)
    
    print(f"\nName: '{name}'\nTime: {total:.6f} s\nSize: {len(encode(values))} bytes")

compare("Compaqt", compaqt.encode, compaqt.decode)
compare("MsgPack", msgpack.packb, msgpack.unpackb)
compare("Pickle", pickle.dumps, pickle.loads)
compare("JSON", json.dumps, json.loads)

