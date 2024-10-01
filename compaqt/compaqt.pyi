# compaqt.pyi

def encode(value: any) -> bytes:
    """Encode a value to bytes.
    
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
    
    Usage:
    >>> original_value = cq.decode(encoded)
    """
    ...

