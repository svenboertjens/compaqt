# Code blocks

This file documents the code blocks in the main `compaqt.c` file.

Code blocks are defined as `/* BLOCK NAME */` in code.


## Contents

- [Globals](#globals)
- [Metadata macros](#metadata-macros)
- [Endianness transformations](#endianness-transformations)
- [Supported datatypes](#supported-datatypes)
- [Datatype conversions](#datatype-conversions)
- [Encoding](#encoding)
- [Decoding](#decoding)
- [Setting methods](#setting-methods)
- [Module definitions](#module-definitions)


## Globals

The globals are variables defined globally across the file.

The globals:
- `avg_item_size`:     Average item size (for container allocations).
- `avg_realloc_size`:  Average size to re-allocate on insufficient allocations.
- `reallocs`:          How often we had to re-allocate in 1 session (used to tweak allocation variables).
- `allocated`:         How much space is allocated to the message buffer.
- `msg`:               The message buffer we're writing to / reading from.
- `offset`:            The current offset we're at in the message buffer.


## Metadata macros

The metadata macros are used to read and write metadata to and from the message buffer, respectively.
These macros increment the message offset for us, unless specified otherwise.

The metadata macros mainly use bit masking to push multiple variables onto a single byte.

The metadata macros:
- `MAX_VLE_METADATA_SIZE`:  The max number of bytes that metadata can take up (used for allocation checks).
- `WR_METADATA_VLE`:        Write metadata with the Variable-Length Encoding (VLE) method.
- `RD_METADATA_VLE`:        Read metadata written by the `WR_METADATA_VLE` method.
- `RD_DTMASK`:              Read the datatype mask from the current offset (does not increment as length data is also encoded on it).
- `RD_DTMASK_GROUP`:        Read the datatype mask when `RD_DTMASK` gives us the GROUP type.


## Endianness transformations

These methods convert number types to little-endian format, if required. They return the original value if transformation is not necessary.

The transformation methods:
- `LL_LITTLE_ENDIAN`:  Converts `long` objects to little-endian format.
- `DB_LITTLE_ENDIAN`:  Converts `double` objects to little-endian format.


## Supported datatypes

The supported, or 'built-in' datatypes each have their own unique bitmask.

The datatype bitmasks:
- `DT_ARRAY`:  Arrays (lists)
- `DT_DICTN`:  Dicts
- `DT_BYTES`:  Bytes
- `DT_STRNG`:  Strings
- `DT_INTGR`:  Integers
- `DT_GROUP`:  Group types: extension mask for datatypes not requiring length metadata
- `DT_BOOLF`:  (group extension) Booleans (false)
- `DT_BOOLT`:  (group extension) Booleans (true)
- `DT_FLOAT`:  (group extension) Floats
- `DT_NONTP`:  (group extension) NoneTypes
- `DT_CMPLX`:  (group extension) Complexes
- `DT_NOUSE`:  This one is not yet used by anything
- `DT_EXTND`:  Extension mask for custom datatypes


## Datatype conversions

Most datatypes have functions to get the length of their value, or write them to/read them from the message buffer.
These functions all increment the offset for us as well.

Datatypes like the NoneType don't have functions for this, due to them being handled differently due to how they're stored (no metadata for example).


## Encoding

These are the methods used for encoding values to bytes.

The helper functions for the encode methods:
- `REALLOC_MSG`:   Reallocate the message buffer with a new size.
- `OFFSET_CHECK`:  Ensures there's enough memory allocated to store the encoded data, reallocs if not so.
- `DT_CHECK`:      Checks whether the found datatype matches the datatype we expected to find.

The main functions used to encode data:
- `encode_container_item`:  Encode an item found inside of container types (list, dict).
- `encode_list`:            Encode a list type.
- `encode_dict`:            Encode a dict type.
- `ENCODE_SINGLE_VALUE`:    Macro for encoding a single value (types that have variable length).
- `encode`:                 The main function for encoding data.


## Decoding

These methods are used to decode encoded bytes and convert them to Python objects.

The helper functions for the decode methods:
- `SET_INVALID_ERR`:  Set an error stating we got an invalid input.
- `OVERREAD_CHECK`:   Check if we're overreading the input (which would mean invalid input).
- `DECODE_ITEM`:      Macro for decoding an item from a container type.

The main functions used to decode data:
- `decode_container_item`:  Decode an item found in a container type.
- `decode`:                 Main function for decoding data.


## Setting methods

There aren't any methods to control settings currently.


## Module definitions

These set up the module and return it to Python for use.

