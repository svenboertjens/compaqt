# compaqt.pyi

def encode(value: any) -> bytes:
    """Encode a value to bytes.
    
    Usage:
    >>> to_encode = ...
    >>> encoded = compaqt.encode(to_encode)
    
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
    
    Usage:
    >>> encoded = cq.encode(...)
    >>> original_value = cq.decode(encoded)
    """
    ...

