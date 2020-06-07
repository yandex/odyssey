#ifndef ODYSSEY_ATTRIBUTE_H
#define ODYSSEY_ATTRIBUTE_H

#include <stddef.h>

char *
read_attribute(char **data, char attr_key);

char *
read_any_attribute(char **data, char *attribute_ptr);

int
read_attribute_buf(char **data,
                   size_t *data_size,
                   char attr_key,
                   char **out,
                   size_t *out_size);

int
read_any_attribute_buf(char **data,
                       size_t *data_size,
                       char *attribute_ptr,
                       char **out,
                       size_t *out_size);

#endif /* ODYSSEY_ATTRIBUTE_H */
