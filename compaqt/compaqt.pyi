# compaqt.pyi

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

class settings:
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

