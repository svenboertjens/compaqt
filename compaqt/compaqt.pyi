# compaqt.pyi

import typing

def encode(value: any) -> bytes:
    """Encode a value to bytes.
    
    Args:
    - `value`:  The value to encode.
    
    Returns the value encoded to bytes.
    
    Usage:
    >>> encoded = compaqt.encode(any_value)
    
    Supported datatypes (excl. custom extensions):
    - bytes
    - str
    - int
    - float
    - complex
    - bool
    - NoneType
    - list
    - dict
    """
    ...

def decode(encoded: bytes) -> any:
    """Decode an encoded bytes object back to the original value.
    
    Args:
    - `encoded`:  The encoded value to decode.
    
    Returns the decoded value.
    
    Usage:
    >>> original_value = compaqt.decode(encoded)
    """
    ...

def validate(encoded: bytes, error_on_invalid: bool=False) -> bool:
    """Validate whether an encoded bytes object is valid
    
    Args:
    - `encoded`:           The encoded value to validate.
    - `error_on_invalid`:  Whether to throw an error if the the value is invalid.
    
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
    - `preserve_file`:  If the current file needs to be preserved and the stream should start at the end of the file. Overrides the `resume_stream` and `stream_offset` args.
    
    Returns an Encoding Stream object to update the stream with.
    
    Usage:
    >>> stream = compaqt.StreamEncoder('myFolder/myFile.ext', list)
    """
    
    def __init__(self, file_name: str, value_type: type, chunk_size: int=1024*1024*8, resume_stream: bool=False, stream_offset: int=0, preserve_file: bool=False) -> self:
        ...
    
    def update(self, value: any, clear_memory: bool=False) -> None:
        """Update an encoding stream by adding new data to it
        
        Args:
        - `value`:         The value to encode. Has to be of the type defined in the `get_encoder` method.
        - `clear_memory`:  Whether to clear the memory chunk after serializing instead of preserving it for the next call.
        
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


class settings:
    """
    Control aspects of the serializer during runtime.
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
    
