# Usage

Here, you can find explanations on how to use `compaqt`.


## Contents

- [Supported datatypes](#Supported-datatypes)
    - [Standard types](#Standard-types)
    - [Custom types](#Custom-types)

- [Basic serialization](#Basic-serialization)
    - [Encode (basic)](#Encode-(basic))
    - [Decode (basic)](#Decode-(basic))

- [Advanced serialization](#Advanced-serialization)


## Supported datatypes

A basic overview of the datatypes supported by `compaqt`.


### Standard types

`compaqt` offers most 'standard' datatypes by default. If a datatype you need is not listed, fear not, and see [Custom types](#Custom-types).

The by default supported datatypes:
- bytes
- str
- int
- float
- complex
- bool
- NoneType
- list
- dict


### Custom types

Unfortunately, `compaqt` does not support custom datatypes yet. This is, however, in active development.


## Basic serialization

This covers the basic serialization methods. For advanced usage, see [Advanced serialization](#Advanced-serialization).


### Encode (basic)

```python
encode(value: any) -> bytes
```

Args:
- `value`: The value to encode to bytes.

Returns the value encoded to bytes.

Example:
```python
# The value we want to encode
value = {
    'Hello,': 'world!',
    'this is a': ['test', 'value']
}

# Encode the value to bytes
encoded = compaqt.encode(value)
```

### Decode (basic)

```python
decode(encoded: bytes) -> any
```

Args:
- `encoded`: The encoded bytes, created by the `encode` method.

Returns the original value.

Example:
```python
# The encoded value (any bytes object created by the `encode` method)
encoded = ...

# Decode the encoded object to retrieve the original value
original_value = compaqt.decode(encoded)
```


## Advanced serialization

This module does not support advanced methods yet.

