# Usage

Here, you can find explanations on how to use `compaqt`.


## Contents

- [Supported datatypes](#supported-datatypes)
    - [Standard types](#standard-types)
    - [Custom types](#custom-types)
- [Basic serialization](#basic-serialization)
    - [Encode (basic)](#encode-(basic))
    - [Decode (basic)](#decode-(basic))
- [Advanced serialization](#advanced-serialization)
- [Validation](#validation)
- [Settings](#settings)
    - [Allocations](#allocations)


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

Unfortunately, `compaqt` does not support custom datatypes yet. This feature is listed on the TODO and will likely be worked on in the near future.


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

This module does not support advanced serialization methods yet. This feature is listed on the TODO and will likely be worked on in the near future.


## Validation

To check whether a bytes object can be decoded correctly by Compaqt, we can use the `validate` function.

```python
validate(encoded: bytes, error_on_invalid: bool=False) -> bool
```

Args:
- `encoded`:          The encoded bytes object to validate.
- `error_on_invalid`: Whether to throw an error if the object is deemed invalid. Defaults to False.

Returns `True` if the object is valid, otherwise returns `False`.

Example:
```python
# The encoded object we want to validate
encoded = ...

# Check whether the encoded object is valid
is_valid = compaqt.validate(encoded)

# Or, throw an error if it fails:
compaqt.validate(encoded, error_on_invalid=True)
```


## Settings

The settings allow us to control certain aspects of the serializer during runtime. These are available through the `compaqt.settings` namespace.


### Allocations

The allocation settings let us decide how much memory to allocate when encoding values.
The two available modes are `manual` and `dynamic` (default), where `manual` lets us define static allocation sizes to always follow, and `dynamic` enables on-the-fly allocation size tweaks based on the input data.


#### Manual allocations

```python
settings.manual_allocations(item_size: int, realloc_size: int) -> None
```

Args:
- `item_size`:    Bytes to allocate per item found in lists/dicts.
- `realloc_size`: Bytes to allocate extra when the current allocation is insufficient.

Example:
```python
# If we only want to serialize values with a size of 6 bytes, we can do this:
compaqt.settings.manual_allocations(6, 18) # The 18 is for re-allocations, if necessary
```


#### Dynamic allocations

```python
settings.dynamic_allocations(item_size: int=..., realloc_size: int=...) -> None
```

Args:
- `item_size`:    Bytes to allocate per item found in lists/dicts. Defaults to the currently set value.
- `realloc_size`: Bytes to allocate extra when the current allocation is insufficient. Defaults to the currently set value.

Example:
```python
# If we want to enable dynamic allocations, and start the item size at 8
compaqt.settings.dynamic_allocations(item_size=8)
```

