# compaqt.pyi
    
def encode(value: any, file_name: str=None) -> bytes | None:
    """Encode a value to bytes.
    
    Args:
    - `value`:     The value to encode.
    - `file_name`:  The file to write encoded data to. By default doesn't write to a file and returns the bytes as a value.
    
    Returns the value encoded to bytes (if not writing to a file).
    
    Usage:
    >>> encoded = compaqt.encode(any_value)
    
    Supported datatypes:
    - bytes
    - str
    - int
    - float
    - bool
    - NoneType
    - list
    - dict
    """
    ...

def decode(encoded: bytes=None, file_name: str=None) -> any:
    """Decode an encoded bytes object back to the original value.
    
    Args:
    - `encoded`:   The encoded value to decode. Overrides `file_name`.
    - `file_name`:  The file to read the data from. Can be given INSTEAD of `encoded`.
    
    Returns the decoded value.
    
    Usage:
    >>> original_value = compaqt.decode(encoded)
    """
    ...

def validate(encoded: bytes=None, file_name: str=None, file_offset: int=0, chunk_size: int=0, err_on_invalid: bool=False) -> bool:
    """Validate whether encoded bytes object are valid
    
    Args:
    - `encoded`:         The encoded value to validate. Overrides `file_name`.
    - `file_name`:        The path to the file to read the data from. Can be given INSTEAD of `encoded`.
    - `file_offset`:     The offset in the file to start reading from.
    - `chunk_size`:      The size of the internal buffer to process data in. Zero means the size of the file (starting from the file offset).
    - `err_on_invalid`:  Whether to throw an error if the the value is invalid.
    
    Returns a boolean on whether the encoded object is valid.
    
    Usage:
    >>> is_valid = compaqt.validate(encoded)
    """
    ...

class StreamEncoder:
    """Create an encoding stream for writing serialized data directly to a file.
    
    Args:
    - `file_name`:      The path to the file to write to.
    - `value_type`:     The type of value to serialize. Can be 'list' or 'dict'.
    - `chunk_size`:     How large the memory buffer for storing the encoded object can be before writing to the file.
    - `resume_stream`:  If the stream was already initialized and you want to continue streaming to it.
    - `file_offset`:    What file position offset to start the stream at.
    - `preserve_file`:  If the current file needs to be preserved and the stream should start at the end of the file. Overrides the `resume_stream` and `file_offset` args.
    
    Returns an Encoding Stream object to update the stream with.
    
    Usage:
    >>> stream = compaqt.StreamEncoder('myFolder/myFile.ext', list)
    """
    
    def __init__(self, file_name: str, value_type: type=list, chunk_size: int=1024*256, resume_stream: bool=False, file_offset: int=0, preserve_file: bool=False) -> self:
        self.start_offset: int = ...
        self.total_offset: int = ...
        ...
    
    def write(self, value: any, clear_memory: bool=False, chunk_size: int=...) -> None:
        """Update an encoding stream by adding new data to it
        
        Args:
        - `value`:         The value to encode. Has to be of the type defined in the `get_encoder` method.
        - `clear_memory`:  Whether to clear the allocated memory chunk after serializing instead of preserving it for the next call.
        - `chunk_size`:    Set the chunk size of the memory chunk. Defaults to the currently set value.
        
        Usage:
        >>> stream.update(['a', 'b', 'c'])
        """
        ...
    
    def finalize(self) -> None:
        """Finalize a stream encoder by freeing its internal buffer and invalidating the Stream Encoder object.
        
        Usage:
        >>> stream.finalize()
        """
        ...

class StreamDecoder:
    """Create a decoding stream for reading and decoding data directly from a file.
    
    Args:
    - `file_name`:    The path to the file to read from.
    - `chunk_size`:   How much memory to allocate for temporarily storing the encoded data from the file.
    - `file_offset`:  What file position offset to start the stream at.
    
    Returns a Decoding Stream object to (progressively) read values with.
    
    Usage:
    >>> stream = compaqt.StreamDecoder('myFolder/myFile.ext')
    """
    
    def __init__(self, file_name: str, chunk_size: int=1024*256, file_offset: int=0) -> self:
        self.total_offset: int = ...
        self.items_remaining: int = ...
        ...
    
    def read(self, num_items: int=..., clear_memory: bool=False, chunk_size: int=...) -> any:
        """Decode values from a decoding stream.
        
        Args:
        - `num_items`:     The number of items to decode. Defaults to all items. Does not throw an error if this value exceeds the number of items. Instead, we can see if the decoder is 'exhausted' with `stream.exhausted`.
        - `clear_memory`:  Whether to clear the allocated memory chunk after serializing instead of preserving it for the next call.
        - `chunk_size`:    Set the chunk size of the memory chunk. Defaults to the currently set value.
        
        Returns the decoded value.
        
        Usage:
        >>> values = stream.read()
        """
        ...
    
    def finalize(self) -> None:
        """Finalize a stream decoder by freeing its internal buffer and invalidating the Stream Decoder object.
        
        Usage:
        >>> stream.finalize()
        """
        ...


class settings:
    """Control aspects of the serializer during runtime.
    """
    
    def manual_allocations(item_size: int, realloc_size: int) -> None:
        """Disable dynamic allocation tweaks and set your own allocation sizes.
        
        Args:
        - `item_size`:     Bytes to allocate per item.
        - `realloc_size`:  Bytes to allocate when the current allocation is insufficient.
        
        Usage:
        >>> compaqt.settings.manual_allocations(32, 64)
        """
        ...
    
    def dynamic_allocations(item_size: int=..., realloc_size: int=...) -> None:
        """Enable dynamic allocations and optionally set the allocation sizes to start off with.
        
        Args:
        - `item_size`:     Bytes to allocate per item.
        - `realloc_size`:  Bytes to allocate when the current allocation is insufficient.
        
        Usage:
        >>> compaqt.settings.dynamic_allocations(item_size=32, realloc_size=64)
        """
        ...

