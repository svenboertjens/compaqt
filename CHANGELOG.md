# Changelog

All changes to the `compaqt` module are documented here.


## [0.4.4] - 2024-10-20

### Fixes:
- Dictionary serialization allocation issues;

### Updates:
- Drop support for complex types (decoding performance purposes);
- Faster decoding speeds;


## [0.4.3] - 2024-10-19

### Fixes:
- Fix mode 2 metadata reading (used for sizes >= 2048);
- Fix memory leak in nested dictionary objects;

### Updates:
- Performance improvements in serialization;
- More efficient mode 2 metadata byte counting;


## [0.4.2] - 2024-10-17

### Fixes:
- Zero-division error in dynamic allocation algorithm;

### Updates:
- Re-add dynamic allocation algorithm;


## [0.4.1] - 2024-10-16

### Fixes:
- Fix support for UTF-8 (previously only worked with ASCII);
- Improved safety and error handling;
- Fix memory leaks from lack of cleanup in error cases;
- Fix strict aliasing rules;
- Fix big-endian conversion to little-endian;

### Updates:
- Support streaming with files:
    * Incrementally read/write data with StreamEncoder/StreamDecoder objects;
    * Directly read/write data with the regular encode/decode methods;
    * Fully compatible and interchangeable with regular serialization;
- Improve args parsing error messages;
- Change metadata length structure for efficiency;
- Change how singular values (values not within a container) are stored;
- Better endianness handling;


## [0.3.2] - 2024-10-04

### Fixes:
- Fix fatal include error from [0.3.1];


## [0.3.1] - 2024-10-04 (DELETED)

Deleted due to an include error.

### Updates:
- Switched from global variables to structs for thread safety (and future streaming and chunk processing update);
- Switched from one big file to multiple files for readability and maintainability purposes;


## [0.3.0] - 2024-10-03

### Fixes:
- Add overread checks on float and complex types;

### Updates:
- Add encoded object validation method;


## [0.2.0] - 2024-10-02

### Updates:
- Add allocation control settings;


## [0.1.2] - 2024-10-1

### Fixes:
- `__doc__` attribute in `compaqt/__init__.py`


## [0.1.1] - 2024-10-1

### Fixes:
- Resolve setup issues from version [0.1.0];

### Updates:
- Modify README, usage paragraph;
- Create docs folder, includes USAGE file;


## [0.1.0] - 2024-9-30

Initial release!

### Updates:
- Basic serialization methods `encode` and `decode`;


## [0.0.1] - 2024-09-30

Experimental version, pre-release.

