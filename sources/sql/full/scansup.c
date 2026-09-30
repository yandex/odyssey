#include <odyssey.h>
#include <pg_compat.h>
#include <postgres_fe.h>
#include <mb/pg_wchar.h>
#include <sql/full/scansup.h>

#include <ctype.h>

static int
pg_mbcliplen_utf8(const char *mbstr, int len, int limit)
{
	int clen = 0;
	const unsigned char *s = (const unsigned char *) mbstr;

	while (len > 0 && *s)
	{
		int l;

		if (*s < 0x80)
			l = 1;
		else if ((*s & 0xE0) == 0xC0)
			l = 2;
		else if ((*s & 0xF0) == 0xE0)
			l = 3;
		else if ((*s & 0xF8) == 0xF0)
			l = 4;
		else
			l = 1;

		if ((clen + l) > limit)
			break;
		clen += l;
		if (clen == limit)
			break;
		len -= l;
		s += l;
	}
	return clen;
}

char *
downcase_truncate_identifier(const char *ident, int len)
{
	return downcase_identifier(ident, len, true);
}

char *
downcase_identifier(const char *ident, int len, bool truncate)
{
	char	   *result;
	int			i;
	bool		enc_is_single_byte;

	result = palloc(len + 1);
	enc_is_single_byte = pg_database_encoding_max_length() == 1;

	for (i = 0; i < len; i++)
	{
		unsigned char ch = (unsigned char) ident[i];

		if (ch >= 'A' && ch <= 'Z')
			ch += 'a' - 'A';
		else if (enc_is_single_byte && IS_HIGHBIT_SET(ch) && isupper(ch))
			ch = tolower(ch);
		result[i] = (char) ch;
	}
	result[i] = '\0';

	if (i >= NAMEDATALEN && truncate)
		truncate_identifier(result, i);

	return result;
}

void
truncate_identifier(char *ident, int len)
{
	if (len >= NAMEDATALEN)
	{
		len = pg_mbcliplen_utf8(ident, len, NAMEDATALEN - 1);
		ident[len] = '\0';
	}
}

int
scanner_isspace(unsigned char c)
{
	if (c == ' ' ||
		c == '\t' ||
		c == '\n' ||
		c == '\r' ||
		c == '\v' ||
		c == '\f')
		return true;
	return false;
}
