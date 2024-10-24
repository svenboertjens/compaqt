# Usage

Here, you can find explanations on how to use `compaqt`.


## Contents

- [Supported datatypes](#supported-datatypes)
    - [Standard types](#standard-types)
    - [Custom types](#custom-types)
- [Basic serialization](#basic-serialization)
    - [Encode](#encode)
    - [Decode](#decode)
- [Streaming](#streaming)
    - [Compatibility](#compatibility)
    - [StreamEncoder](#streamencoder)
    - [StreamDecoder](#streamdecoder)
- [Validation](#validation)
- [Settings](#settings)
    - [Allocations](#allocations)


## Supported datatypes

A basic overview of the datatypes supported by `compaqt`.


### Standard types

These datatypes are supported by Compaqt:

- bytes
- str
- int
- float
- bool
- NoneType
- list
- dict


### Custom types

Unfortunately, `compaqt` does not support custom datatypes yet. This feature is listed as a to-do.


## Basic serialization

This covers the basic serialization methods.


### Encode

```python
encode(value: any, file_name: str=None) -> bytes | None
```

Args:
- `value`:    The value to encode to bytes.
- `file_name`: The file to write the data to.

* `value`:
The value to encode to bytes.

* `file_name`:
The file to write the data to, *instead* of returning a bytes object with the encoded data.

Returns the value encoded to bytes if file_name is not given, otherwise returns None.


### Decode

```python
decode(encoded: bytes=None, file_name: str=None) -> any
```

* `encoded`:
The encoded value to validate. Overrides `file_name`.

* `file_name`:
The file to read and decode the data from.

Returns the decoded value.


## Validation

To check whether a bytes object can be decoded correctly by Compaqt, we can use the `validate` function.

```python
validate(encoded: bytes=None, file_name: str=None, file_offset: int=0, chunk_size: int=0, err_on_invalid: bool=False) -> bool
```

* `encoded`:
The encoded value we want to validate. This overrides the `file_name` argument.

* `file_name`:
The name of the file to validate data from.

* `* file_offset`:
The offset of the file to start reading from.

* `* chunk_size`:
The amount of bytes of the internal buffer to load data from the file into. If set to zero, use the size of the whole file (starting from `file_offset`).

* `err_on_invalid`:
Whether to throw an error if the data to validate does not appear valid.

* Note: Arguments marked with a `*` **only** do something when the `file_name` argument is given.

Returns `True` if the object is valid, otherwise returns `False`.


## Streaming

Streaming allows us to serialize data directly to and from files. Unlike the more basic file functionality provided by the `encode` and `decode` method, this is much more flexible for file management. This also uses chunk processing internally to contain memory usage, and supports incrementally feeding data instead of having to serialize data all in one go.

* Note: `StreamEncoder` and `StreamDecoder` will be referred to as 'encoder' and 'decoder', respectively.


### Compatibility

Streaming data is fully compatible with the `encode`/`decode` methods. You can seamlessly switch between them without modifying function arguments. In special cases, such as continuing streams with `resume_stream=True`, you’ll need to adjust the `stream_compatible` flag.


### StreamEncoder

The `StreamEncoder` object lets us write data to a file.


#### Creation

To create an encoder, we can use the `StreamEncoder` method. This returns an encoder which can be used to write to the specified file.

```python
StreamEncoder(file_name: str, value_type: type=list, chunk_size: int=1024*256, resume_stream: bool=False, file_offset: int=0, preserve_file: bool=False) -> StreamEncoder
```

* `file_name`:
Specifies what file to write to. The filename **cannot** be changed after creating the object.

* `value_type`:
The 'upper' value type, which can be a list or a dict. For clarity, think of how JSON needs to be within a dict structure. Values within this structure can be anything still.

* `chunk_size`:
The amount of bytes to allocate for the internal buffer. Larger sizes use more memory, but require less buffer refreshes and file writes. This value **can** be changed after creation.

* `resume_stream`:
Whether we want to resume a stream in a file. This is used to continue streaming to a file after the original encoder object can no longer be used, keeping the already written data intact.

* `file_offset`:
The offset to start writing to in the file. This is used to preserve any data underneath the offset position. If resuming a stream, set this argument to where the original stream started, not where it wrote its last value.

* `preserve_file`:
Whether to preserve the current file contents and start writing to the end of the file. This overrides the `resume_stream` and `file_offset` arguments.

Returns an encoder object.


#### Writing

The `write` method lets us write data with the encoder.

```python
write(value: any, clear_memory: bool=False, chunk_size: int=...) -> None
```

* `value`:
The value to write to the stream. This value has to be the same type as the `value_type` provided at the creation of the encoder.

* `clear_memory`:
Whether to free the internal memory buffer after writing, instead of preserving it for the next write.

* `chunk_size`:
The amount of bytes to allocate for the internal buffer. Replaces the initially set chunk size.


#### Finalization

It's not strictly necessary to finalize the encoder. The encoded data does not need any additional changes after writing data and can be used at any time. The `finalize` method can be used to prematurely clean up the internal buffer and other internal data, although this also automatically happens once the encoder gets removed by the garbage collector. This is useful if the value won't get destroyed but is no longer needed, but can be omitted in regular use cases.

```python
finalize() -> None
```

Calling this function also invalidates the encoder object, making it invalid for further usage.


#### Class variables

* `start_offset`:
The offset the encoder started writing to. Useful if it's unknown, such as with the `preserve_file` argument set to `True`.

* `total_offset`:
The total offset of the encoder. This points to the offset directly after the last written data.


### StreamDecoder

The `StreamDecoder` object lets us read encoded data from a file.


#### Creation

To create an decoder, we can use the `StreamDecoder` method. This returns a decoder object which can be used to read and decode data from the specified file.

```python
StreamDecoder(file_name: str, chunk_size: int=1024*256, file_offset: int=0) -> StreamDecoder
```

* `file_name`:
Specifies what file to read from. The filename **cannot** be changed after creating the object.

* `chunk_size`:
The amount of bytes to allocate for the internal buffer. Larger sizes use more memory, but require less buffer refreshes and file reads. This value **can** be changed after creation.

* `file_offset`:
The offset to start reading from in the file. This is used to read from a specific offset if we previously wrote the data to an offset in the file.

Returns a decoder object.


#### Reading

def read(self, num_items: int=..., clear_memory: bool=False, chunk_size: int=...) -> None:
        """Decode values from a decoding stream.
        
        Args:
        - `num_items`:     The number of items to decode. Defaults to all items. Does not throw an error if this value exceeds the number of items. Instead, we can see if the decoder is 'exhausted' with `stream.exhausted`.
        - `clear_memory`:  Whether to clear the allocated memory chunk after serializing instead of preserving it for the next call.
        - `chunk_size`:    Set the chunk size of the memory chunk. Defaults to the currently set value.
        
        Usage:
        >>> values = stream.read()
        """
        ...

The `read` method lets us read and decode data with the decoder.

```python
read(num_items: int=..., clear_memory: bool=False, chunk_size: int=...) -> any
```

* `num_items`:
The number of items to decode. This defaults to decoding all remaining items. No error is thrown when this value exceeds the remaining number of items.

* `clear_memory`:
Whether to free the internal memory buffer after reading, instead of preserving it for the next read.

* `chunk_size`:
The amount of bytes to allocate for the internal buffer. Replaces the initially set chunk size.

Returns the decoded data.


#### Finalization

Just like with encoder objects, it's not necessary to explicitly finalize a decoder. For a more detailed explanation, see [StreamEncoder](#streamencoder) -> Finalization.

```python
finalize() -> None
```

Calling this function invalidates the decoder object, making it invalid for further use.


#### Class variables

* `total_offset`:
The total offset the decoder is at in the file currently.

* `items_remaining`:
The number of items remaining, that have not yet been decoded.


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

