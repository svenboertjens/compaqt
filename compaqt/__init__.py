__version__ = "0.1.1"
__author__ = "Sven Boertjens"
__license__ = "BSD-3-Clause"
__url__ = "https://github.com/svenboertjens/compaqt"
__doc__ = open("docs/USAGE.md").read()

try:
    from .compaqt import encode, decode
except ImportError as e:
    raise ImportError(f"Failed to fetch methods from the `compaqt` file: {e}")