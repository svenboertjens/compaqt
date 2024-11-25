// This file contains typedefs used throughout files

#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#include <Python.h>

/*  Typedef for functions that do buffer checks while encoding/decoding.
 *  These functions return 1 on error and set an error message.
 *  
 *  Encoding:
 *  The functions accept an `encode_t` for the encoding data and a `size_t` for the extra size being added to the buffer offset.
 *  
 *  Decoding:
 *  The functions accept a `decode_t` for the decoding data and a `size_t` for the extra size needed after the buffer offset.
 */
typedef int (*bufcheck_t)(void *, const size_t);


/*  Holds data of a reference buffer. Reference buffers are used for referenced str/bytes types to avoid copying data.
 *  The base can be both a PyObject and an allocated buffer. In case of a PyObject (is_PyOb = 1), DECREF `base`. Otherwise free `base`.
 */
typedef struct {
    void *base;    // Referenced PyObject / allocated buffer.
    size_t refcnt; // Reference count of objects referencing this buffer.
    int is_PyOb;   // Indicates if `base` is a PyObject.
} bufdata_t;


// Key-value pair struct for the hash table
typedef struct {
    PyTypeObject *key;
    PyObject *val;
} keyval_t;

// Maximum number of slots in the hash table
#define HASH_TABLE_SLOTS 32

typedef struct {
    uint8_t offsets[HASH_TABLE_SLOTS]; // Holds the hash table offsets based on the hash value
    uint8_t lengths[HASH_TABLE_SLOTS]; // Holds the chain length of each hash value
    uint8_t idxs[HASH_TABLE_SLOTS];    // Holds the indexes to the `keyvals` pairs, indexed based on the hash
    keyval_t keyvals[]; // Key-value pairs of the hash
} hash_table_t;

// Usertypes encode object struct
typedef struct {
    PyObject_HEAD
    hash_table_t *table; // Hash table containing all data
} utypes_encode_ob;

// Usertypes decode object struct
typedef struct {
    PyObject_HEAD
    PyObject **reads; // The read functions of the types
} utypes_decode_ob;


/*  Holds data for encoding objects to bytes.
 */
typedef struct {
    char *base;       // The buffer base, pointing to the bottom of the buffer (for freeing).
    char *offset;     // The offset buffer, pointing to the current offset in the buffer.
    char *max_offset; // Points to the address directly after the last allocated byte.

    bufcheck_t bufcheck;      // Function to refresh the buffer if necessary.
    utypes_encode_ob *utypes; // Holds user type objects. Is NULL if not used.
} encode_t;

/*  Holds data for decoding bytes to an object.
 */
typedef struct {
    char *base;       // The buffer base, pointing to the bottom of the buffer (for freeing).
    char *offset;     // The offset buffer, pointing to the current offset in the buffer.
    char *max_offset; // Points to the address directly after the last allocated byte.

    bufdata_t *bufd;          // Points to a reference buffer data struct. Is NULL if not used.
    bufcheck_t bufcheck;      // Function to check if enough bytes are remaining or if the buffer needs to be refreshed.
    utypes_decode_ob *utypes; // Holds user type objects. Is NULL if not used.
} decode_t;


/*  Encode struct for regular encoding.
 */
typedef struct {
    // `encode_t` data
    char *base;
    char *offset;
    char *max_offset;
    bufcheck_t bufcheck;
    utypes_encode_ob *utypes;

    // `reg_encode_t` data
    size_t reallocs; // Keep track of re-allocations for dynamic allocation tweaks
} reg_encode_t;

/*  Holds file data for streaming objects
*/
typedef struct {
    FILE *file;          // The currently opened file
    char *filename;      // The filename
    size_t nitems;       // Number of items written/read
    PyTypeObject *type;  // Object type (list or dict)
    size_t chunk_size;   // The chunk size for chunk processing
    size_t start_offset; // The start offset in the file
    size_t curr_offset;  // The current offset in the file
} filedata_t;

typedef struct {
    // `encode_t` data
    char *base;
    char *offset;
    char *max_offset;
    bufcheck_t bufcheck;
    utypes_encode_ob *utypes;

    // `filedata_t` data
    FILE *file;
    char *filename;
    size_t nitems;
    PyTypeObject *type;
    size_t chunk_size;
    size_t start_offset;
    size_t curr_offset;
} stream_encode_t;

typedef struct {
    // `decode_t` data
    char *base;
    char *offset;
    char *max_offset;
    bufdata_t *bufd;
    bufcheck_t bufcheck;
    utypes_decode_ob *utypes;

    // `filedata_t` data
    FILE *file;
    char *filename;
    size_t nitems;
    PyTypeObject *type;
    size_t chunk_size;
    size_t start_offset;
    size_t curr_offset;
} stream_decode_t;


#endif // TYPEDEFS_H