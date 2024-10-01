# Compaqt

A serializer that aims for compactness and performance.


## Installation

To install this module for Python, simply run this:
`pip install compaqt`


## About this serializer

`Compaqt` is a serializer designed to strike a balance between compact data representation and performance. According to benchmarks compared to other existing serialization methods, this one performs well in both compactness and (de-)serialization speed.

In an attempt to keep the memory usage low, Compaqt attempts to predict how much memory to allocate before serializing data, by dynamically learning based on previously received data. This is to avoid over-allocating memory by large amounts, while also reducing re-allocations. This - besides the lower memory footprint - also enhances performance due to the ease of upfront allocation and minimal re-allocations during the process.

The serializer currently supports the following datatypes:
- bytes
- str
- int
- float
- complex
- bool
- NoneType
- list
- dict


## Usage

Encoding (serializing):
```python
# The value we want to encode
value = {
    'Hello,': 'world!',
    'this is a': ['test', 'value']
}

# Call the `encode` method to encode the value
encoded = compaqt.encode(value)

# Now, `encoded` contains the value serialized to bytes
```

Decoding (de-serializing):
```python
# Our encoded value (generated from the value from 'Encoding' above)
encoded = b'!cHello,cworld!\x93this is a CtestSvalue'

# call the `decode` method to decode the value
value = compaqt.decode(encoded)

# Now, `value` contains the original value
```

For further usage or other documentation, see the [docs](https://github.com/svenboertjens/compaqt/tree/main/docs).


## License

This project is licensed under the BSD-3-Clause License. See the [LICENSE](https://github.com/svenboertjens/compaqt/blob/main/LICENSE.md) file for details.


## Contact

Feel free to reach out via the GitHub repository of this module ([github](https://github.com/svenboertjens/compaqt.git)) or reach out by mail ([Sven Boertjens](mailto:boertjens.sven@gmail.com)).

