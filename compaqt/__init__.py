__author__ = "Sven Boertjens"
__license__ = "BSD-3-Clause"
__url__ = "https://github.com/svenboertjens/compaqt"
__doc__ = "For usage details, see <https://github.com/svenboertjens/compaqt/blob/main/USAGE.md> or consult the USAGE file directly from the module directory"

from . import py_fallback as __compaqt_Py
from .py_fallback.regular import encode, decode
from .py_fallback.validate import validate
from .py_fallback.stream import StreamEncoder, StreamDecoder
from .py_fallback import custom_types as types
from .py_fallback import settings

__compaqt_Py.encode = encode
__compaqt_Py.decode = decode
__compaqt_Py.validate = validate
__compaqt_Py.StreamEncoder = StreamEncoder
__compaqt_Py.StreamDecoder = StreamDecoder
__compaqt_Py.types = types
__compaqt_Py.settings = settings

try:
    from . import compaqt as __compaqt_C
    from .compaqt import encode, decode, settings, StreamEncoder, StreamDecoder, validate, types

    __compaqt_C.encode = encode
    __compaqt_C.decode = decode
    __compaqt_C.settings = settings
    __compaqt_C.StreamEncoder = StreamEncoder
    __compaqt_C.StreamDecoder = StreamDecoder
    __compaqt_C.validate = validate
    __compaqt_C.types = types
except ModuleNotFoundError as e:
    __compaqt_C = None
    print("Failed to import Compaqt C implementation, using the Python fallback")

