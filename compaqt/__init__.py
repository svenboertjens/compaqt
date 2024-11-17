__author__ = "Sven Boertjens"
__license__ = "BSD-3-Clause"
__url__ = "https://github.com/svenboertjens/compaqt"
__doc__ = "For usage details, see <https://github.com/svenboertjens/compaqt/blob/main/USAGE.md> or consult the USAGE file directly from the module directory"

try:
    from .compaqt import encode, decode, settings, StreamEncoder, StreamDecoder, validate, types

except Exception as e:
    raise e
    from .py_fallback.regular import encode, decode
    from .py_fallback.validate import validate
    from .py_fallback.stream import StreamEncoder, StreamDecoder
    from .py_fallback import custom_types as types
    from .py_fallback import settings
    
    # Set a warning as the C implementation couldn't be found
    from warnings import warn
    warn(
        "Failed to import Compaqt C implementation, using the Python fallback. Performance may be degraded.",
        ImportWarning
    )

