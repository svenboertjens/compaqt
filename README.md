# Compaqt

A compact serializer aiming for flexibility and performance.


## What is this?

Compaqt is an efficient serializer that aims for speed and compactness. Its purpose is similar to other libraries like JSON, except Compaqt uses a binary format to keep the data sizes low and the performance high.

This library also aims to be flexible and has an easy-to-use API, providing methods for directly using files, chunk processing, and data validation.

Besides the 'standard' types, this serializer supports custom types and lets you assign your own functions to serialize and de-serialize custom types.


## How do I use it?

For 'simple' serialization, this library provides the `encode` and `decode` methods. These methods directly return the encoded data as bytes or the value decoded from encoded data, respectively.
Here's a simple example on how this works:

```python
import compaqt

# The value we want to serialize, can be anything
value = ...

# We can just use the `encode` method to serialize it, that's all we have to do!
encoded = compaqt.encode(value)

# Do stuff
...

# Now, we want to retrieve the value we originally had, which we currently hold in the `encoded` variable
# For this, all we have to do is use the `decode` method:
value = compaqt.decode(encoded)

# Now, `value` holds the original value, exactly as it was when we encoded it earlier
```

If we want to write the result to a file, and later read it from the file, we can use the optional `file_name` argument:

```python
# The file we want to write data to
file_name = 'dir/file.bin'

# Pass the filename to the function to write it to said file, instead of having the function return the bytes back directly
compaqt.encode(value, file_name=file_name)

# Do stuff
...

# Now, we want to read the value back from the file.
# We only have to give `decode` the `file_name` argument this time, no need to pass anything else!
value = compaqt.decode(file_name=file_name)
```

For more advanced streaming functionality, we can use the `StreamEncoder` and `StreamDecoder` classes. These support incremental reading and writing, and internally use chunk processing to optimize memory usage.

Apart from serialization, if we need to be sure that some bytes are valid, we can verify the validity of them using the `validate` method. This supports both direct verification and through streaming, and for streaming also supports all advanced file management features from the `StreamEncoder` and `StreamDecoder` objects. This method also has *optional* chunk processing when streaming.


For further details on how to use this library, please consult the [USAGE](https://github.com/svenboertjens/compaqt/blob/main/USAGE.md).


## Installation

To install this module for Python, simply run this:
`pip install compaqt`

This library automatically detects big-endian systems and applies endianness conversions to keep data valid across architectures. If you're using a big-endian system but do not want the endianness conversions for whatever reason, set the enviroment variable `SET_LITTLE_ENDIAN` before installing the library. As per tests, these conversions bring no noticeable performance losses as they typically use intrinsics (for GCC/Clang and MSVC).

Setting environment variables is done as follows:
- On Unix: `export <VARIABLE_NAME>=1`
- On Windows: `set <VARIABLE_NAME>=1`

So if we want to enforce aliasing for example, we can do so like this:
- On Unix: `export SET_LITTLE_ENDIAN=1 && pip install compaqt`
- On Windows: `set SET_LITTLE_ENDIAN=1 && pip install compaqt`


## License

This project is licensed under the BSD-3-Clause License. See the [LICENSE](https://github.com/svenboertjens/compaqt/blob/main/LICENSE.md) file for details.


## Contact

Feel free to reach out via the [GitHub repo](https://github.com/svenboertjens/compaqt.git) of this module or reach out by [mail](mailto:boertjens.sven@gmail.com).

