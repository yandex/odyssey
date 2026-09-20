#include <odyssey.h>
#include <pg_compat.h>
#include <sql/full/nodes.h>
#include <sql/full/ast.h>
#include <errno.h>
#include <limits.h>

int32
pg_strtoint32_safe(const char *s, Node *escontext)
{
	const char *ptr = s;
	uint32 tmp = 0;
	bool neg = false;
	unsigned char digit;

	if (*ptr == '-')
	{
		ptr++;
		neg = true;
	}

	digit = (*ptr - '0');
	if (digit < 10)
	{
		ptr++;
		tmp = digit;
	}
	else
		goto slow;

	for (;;)
	{
		digit = (*ptr - '0');
		if (digit >= 10)
			break;
		ptr++;
		if (unlikely(tmp > (uint32)(PG_INT32_MAX / 10)))
			goto out_of_range;
		tmp = tmp * 10 + digit;
	}

	if (unlikely(*ptr != '\0'))
		goto slow;

	if (neg)
	{
		if (unlikely(tmp > (uint32)PG_INT32_MAX + 1))
			goto out_of_range;
		return -((int32) tmp);
	}

	if (unlikely(tmp > PG_INT32_MAX))
		goto out_of_range;

	return (int32) tmp;

slow:
	{
		unsigned long longval;
		char *endptr;

		errno = 0;
		longval = strtoul(s, &endptr, 0);
		if (endptr == s || *endptr != '\0')
			goto out_of_range;
		if (errno != 0)
			goto out_of_range;
		if (neg)
		{
			if (longval > (unsigned long)PG_INT32_MAX + 1)
				goto out_of_range;
			return -(int32) longval;
		}
		else
		{
			if (longval > PG_INT32_MAX)
				goto out_of_range;
			return (int32) longval;
		}
	}

out_of_range:
	if (escontext)
	{
		((ErrorSaveContext *) escontext)->error_occurred = true;
	}
	return 0;
}

int
pg_mbstrlen_with_len(const char *mbstr, int limit)
{
	int len = 0;
	const unsigned char *s = (const unsigned char *) mbstr;

	while (limit > 0 && *s)
	{
		if (*s < 0x80)
		{
			s++;
			limit--;
		}
		else if ((*s & 0xE0) == 0xC0)
		{
			s += 2;
			limit -= 2;
		}
		else if ((*s & 0xF0) == 0xE0)
		{
			s += 3;
			limit -= 3;
		}
		else if ((*s & 0xF8) == 0xF0)
		{
			s += 4;
			limit -= 4;
		}
		else
		{
			s++;
			limit--;
		}
		len++;
	}
	return len;
}

int
pg_database_encoding_max_length(void)
{
	return 4;
}

void
pg_unicode_to_server(char32_t c, unsigned char *buf)
{
	if (c < 0x80)
	{
		buf[0] = (unsigned char) c;
	}
	else if (c < 0x800)
	{
		buf[0] = 0xC0 | ((c >> 6) & 0x1F);
		buf[1] = 0x80 | (c & 0x3F);
	}
	else if (c < 0x10000)
	{
		buf[0] = 0xE0 | ((c >> 12) & 0x0F);
		buf[1] = 0x80 | ((c >> 6) & 0x3F);
		buf[2] = 0x80 | (c & 0x3F);
	}
	else
	{
		buf[0] = 0xF0 | ((c >> 18) & 0x07);
		buf[1] = 0x80 | ((c >> 12) & 0x3F);
		buf[2] = 0x80 | ((c >> 6) & 0x3F);
		buf[3] = 0x80 | (c & 0x3F);
	}
}

int
pg_get_client_encoding(void)
{
	return 6; /* PG_UTF8 */
}

bool
pg_verifymbstr(const char *mbstr, int len, bool noError)
{
	(void)mbstr;
	(void)len;
	(void)noError;
	return true;
}
