#pragma once

#include <stddef.h>
#include <stdbool.h>

char *downcase_truncate_identifier(const char *ident, int len);
char *downcase_identifier(const char *ident, int len, bool truncate);
void truncate_identifier(char *ident, int len);
int scanner_isspace(unsigned char c);
