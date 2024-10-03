from customtest import Test
import compaqt

test = Test()

values = {
    "int": [
        0,
        1,
        -1,
        10**1000,
        -10**1000,
        128,
        129,
        256,
        257,
        -128,
        -129,
        -256,
        -257
    ],
    "str": [
        '',
        'abcde',
        'Hello, world!',
        'abcde' * 10000
    ],
    "bytes": [
        b'',
        b'abcde',
        b'Hello, world!',
        b'abcde' * 10000
    ],
    "float": [
        0.0,
        1.0,
        1e-1000,
        1e1000,
        128.3,
        2008.86
    ],
    "complex": [
        2j + 3,
        0j + 0,
        1e-1000j + 1e1000
    ],
    "bool": [
        True,
        False
    ],
    "NoneType": [
        None
    ],
    "list": [
        ['hello,' 'world!'],
        [],
        [[[[[[[[[[]]]]]]]]]]
    ],
    "dict": [
        {'hello,': 'world!'},
        {1: {2: {3: {4: {5: {'nested': 'dict'}}}}}},
        {}
    ]
}

for tp, vals in values.items():
    print(f"\nType '{tp}'")
    test.test_values(vals)

