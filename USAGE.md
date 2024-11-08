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
- [Implementations](#implementations)


## Supported datatypes

Compaqt supports most 'default' types, which is sufficient for most use cases. If you need to use other types, you can directly pass them to the serialization methods along with a custom type object (more on those below).


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

Custom types can be used to handle data of types not supported by Compaqt itself. To use custom types, we can create an object that'll hold the function to call per custom type along with its assigned ID.

The objects needed for encoding data is separate from the object for decoding, as this approach significantly benefits memory usage.

The methods used to create custom type objects are located in `compaqt.types`.


#### Custom Write Types

The `CustomWriteTypes` is used for encoding custom types.

```python
types.encoder_types(custom_types: dict[type: tuple[int, function]]) -> CustomWriteTypes
```

* `custom_types`:
A dict that holds the custom types, the functions to encode them, and their ID. This dict is structured as follows:

```python
custom_types = {
    ID1: (type1, function1),
    ID2: (type2, function2),
    ...
}
```

The unique ID is the key in the dict, and the value belonging to the type should be a tuple containing the custom type and the function used to encode the custom type.

The type and function in the tuple are **not** interchangeable. The ID should come first, and then the function.

The function for encoding the value will receive the value of the custom type (which has been type checked for you before calling), and should return a bytes object of the value that will later be used to decode the value in your decode function.


#### Custom Read Types

The `CustomReadTypes` is used for decoding custom types.

```python
types.decoder_types(custom_types: dict[int: function]) -> CustomReadTypes
```

* `custom_types`:
A dict that holds the functions to decode custom types and the unique ID used for the type when encoding with a `CustomWriteTypes` object. It's structured as follows:

```python
custom_types = {
    ID1: function1,
    ID2: function2,
    ...
}
```

The ID is the key in the dict, and the value paired to the ID is the function used to decode the encoded value of your custom type.

The function for decoding the value will receive the bytes as they were encoded by the custom encode function. The function is allowed to return **anything**, it does not strictly have to be the custom type.

* Note: The ID that was used for a custom type in `CustomWriteTypes` **has** to be the same ID as used in the `CustomReadTypes`. Otherwise, functions won't receive the correct data, or the encoded data might be seen as invalid, throwing an error.


#### Usage example: Custom types

This example shows how custom types can be used to work with custom types.

```python
import compaqt

# This is the custom type we want to serialize
class CustomString:
    
    def __init__(self, value: str):
        self.value = value

# This is how the function for serializing this custom type would look:
def encode_CustomString(obj: CustomString):
    # The `value` of the custom type is all we want to save here,
    # so return that as bytes:
    
    return obj.value.encode()

# Create a custom types object for encoding to use our custom type:
encode_types = compaqt.types.encoder_types({
    0: (CustomString, encode_CustomString)
})

# This is the value we want to serialize
value = [
    CustomString('abcde'),
    CustomString('12345')
]

# Serialize the value, passing the custom types object along
encoded = compaqt.encode(value, custom_types=encode_types)


# Now, we want to decode the value again. But just passing this object
# to the `decode` method will cause issues. So we have to create
# a custom types object for decoding, with our own decode function.


# This is the function for decoding our custom type:
def decode_CustomString(data: bytes):
    # The `data` variable holds the string we previously encoded in our
    # custom encode function for the CustomString type. We want to get
    # a CustomString object back, so decode `data` and create a CustomString:
    
    value = data.decode()
    return CustomString(value)

# Create a custom types object for de-serializing the encoded data:
decode_types = compaqt.types.decoder_types({
    # Use the same ID as before to correctly identify it
    0: decode_CustomString
})

# De-serialize the encoded data to get our value back:
decoded = compaqt.decode(encoded, custom_types=decode_types)


# Now, `decoded` holds the exact same as `value` holds, using our custom type!
```


## Basic serialization

The basic serialization methods are sufficient for most use cases.

Using advanced serialization methods should be considered when:

- Memory usage is a concern;
- Data needs to be (de-)serialized incrementally;
- Using files that require complex setup;

The basic methods are generally easier to use and have less performance overhead due to simplicity.


### Encode

```python
encode(value: any, file_name: str=None, stream_compatible: bool=False, custom_types: CustomWriteTypes=None) -> bytes | None
```

* `value`:
The value to encode to bytes.

* `file_name`:
The file to write the data to, *instead* of returning a bytes object with the encoded data.

Returns the value encoded to bytes if file_name is not given, otherwise returns None.


### Decode

```python
decode(encoded: bytes=None, file_name: str=None, custom_types: CustomReadTypes=None) -> any
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

* Note: Allocation settings are not implemented in the Python fallback and will throw an exception when used.


#### Manual allocations

```python
settings.manual_allocations(item_size: int, realloc_size: int) -> None
```

* `item_size`:
The amount of bytes to allocate per item found in a list or dict. A key-value pair in a dict counts as 2 items.

* `realloc_size`:
The amount of bytes to allocate as extra space when the currently allocated buffer is insufficient. This size is also added to the initial allocation size when starting an encoding process.


#### Dynamic allocations

```python
settings.dynamic_allocations(item_size: int=..., realloc_size: int=...) -> None
```

* `item_size`:
The amount of bytes to allocate per item found in a list or dict. One key-value pair in a dict counts as 2 items.

* `realloc_size`:
The amount of bytes to allocate as extra space when the currently allocated buffer is insufficient. This size is also added to the initial allocation size when starting an encoding process.

* Note: These values default to the currently set values when not provided.


## Implementations

Compaqt provides both a C implementation and a Pure-Python version. The C implementation is used by default, when available.

If you want to use a specific implementation, you can use `compaqt.__compaqt_C` and `compaqt.__compaqt_Py`. The methods are available through that namespace, so for example the Python implementation's `encode` can be accessed through `compaqt.__compaqt_Py.encode(...)`.

The `__compaqt_C` namespace will be set to `None` when using the Python implementation due to the C version not being found.

By default, Compaqt will set an `ImportWarning` (not throwing an exception) when the C implementation isn't found. This warning can be suppressed as follows:

```python
import warnings
warnings.filterwarnings("ignore", category=ImportWarning)
```

