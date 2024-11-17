#ifndef TYPEMASKS_H
#define TYPEMASKS_H

#define DT_ARRAY (unsigned char)(0) // Arrays
#define DT_DICTN (unsigned char)(1) // Dicts
#define DT_BYTES (unsigned char)(2) // Binary
#define DT_STRNG (unsigned char)(3) // Strings
#define DT_INTGR (unsigned char)(4) // Integers

// Group datatypes stores datatypes not requiring length metadata (thus can take up whole byte)
#define DT_GROUP (unsigned char)(5) // Group datatype mask for identifying it's a grouped type
#define DT_BOOLF DT_GROUP | (unsigned char)(0 << 3) // False Booleans
#define DT_BOOLT DT_GROUP | (unsigned char)(1 << 3) // True Booleans
#define DT_FLOAT DT_GROUP | (unsigned char)(2 << 3) // Floats
#define DT_NONTP DT_GROUP | (unsigned char)(3 << 3) // NoneTypes

#define DT_EXTND (unsigned char)(6) // Custom types

#define DT_NOUSE (unsigned char)(7) // Not in use for anything

#endif // TYPEMASKS_H