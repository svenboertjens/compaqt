import compaqt as cq

values = [
    b'Hello,',
     'world!',
    b'abcdefg' * 1000,
     'abcdefg' * 1000,
    2008,
    -17,
    0,
    9e999,
    3.142,
    0.0,
    9e-999,
    2j + 3,
    9e-999j + 9e999,
    [
        'a', 'b', 'c',
        1234567891011
    ],
    {
        'd': 'e',
        'f': ['g', 'h', None]
    },
    True,
    False,
    None,
    [
        True,
        False,
        None
    ],
    [],
    {},
    [[[[{}]]]]
]

def test(i, value):
    if cq.decode(cq.encode(value)) != value:
        global fails
        fails += 1
        print(f'Item {i} failed: \'{value}\'')

fails = 0

for i, value in enumerate(values):
    test(i, value)

test('\'full set\'', values)

if fails == 0:
    print('No issues found')
else:
    print(f'Failed {fails} times')

