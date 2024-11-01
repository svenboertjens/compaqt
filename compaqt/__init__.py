__author__ = "Sven Boertjens"
__license__ = "BSD-3-Clause"
__url__ = "https://github.com/svenboertjens/compaqt"
__doc__ = "For usage details, see <https://github.com/svenboertjens/compaqt/blob/main/USAGE.md> or consult the USAGE file directly from the module directory"

try:
    from .compaqt import encode, decode, settings, StreamEncoder, StreamDecoder, validate, types
except ImportError as e:
    raise ImportError(f"Failed to fetch methods from the `compaqt` file: {e}")