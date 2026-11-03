#pragma once

/* PostgreSQL protocol versions and startup request codes. */

#define PG_PROTOCOL(m, n) (((m) << 16) | (n))
#define PG_PROTOCOL_MAJOR(v) ((int)((v) >> 16))
#define PG_PROTOCOL_MINOR(v) ((int)((v) & 0x0000ffff))
#define NEGOTIATE_SSL_CODE PG_PROTOCOL(1234, 5679)
#define NEGOTIATE_GSS_CODE PG_PROTOCOL(1234, 5680)
#define CANCEL_REQUEST_CODE PG_PROTOCOL(1234, 5678)
#define PG_PROTOCOL_LATEST PG_PROTOCOL(3, 2)
#define PG_PROTOCOL_EARLIEST PG_PROTOCOL(3, 0)
