# This file contains the datatype masks

DT_ARRAY = 0
DT_DICTN = 1
DT_BYTES = 2
DT_STRNG = 3
DT_INTGR = 4
DT_GROUP = 5
DT_EXTND = 6

DT_BOOLF = DT_GROUP | (0 << 3)
DT_BOOLT = DT_GROUP | (1 << 3)
DT_FLOAT = DT_GROUP | (2 << 3)
DT_NONTP = DT_GROUP | (3 << 3)

