#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include "metadata.h"
#include "types/custom.h"

int encode_item(buffer_t *b, PyObject *item, custom_types_wr_ob *custom_ob, buffer_check_t offset_check);
PyObject *decode_item(buffer_t *b, custom_types_rd_ob *custom_ob, buffer_check_t overread_check);

#endif // SERIALIZATION_H