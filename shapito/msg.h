#ifndef SHAPITO_MSG_H
#define SHAPITO_MSG_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef enum
{
	SHAPITO_FE_TERMINATE        = 'X',
	SHAPITO_FE_PASSWORD_MESSAGE = 'p',
	SHAPITO_FE_QUERY            = 'Q',
	SHAPITO_FE_FUNCTION_CALL    = 'F',
	SHAPITO_FE_PARSE            = 'P',
	SHAPITO_FE_BIND             = 'B',
	SHAPITO_FE_DESCRIBE         = 'D',
	SHAPITO_FE_EXECUTE          = 'E',
	SHAPITO_FE_SYNC             = 'S',
	SHAPITO_FE_CLOSE            = 'C',
	SHAPITO_FE_COPY_DATA        = 'd',
	SHAPITO_FE_COPY_DONE        = 'c',
	SHAPITO_FE_COPY_FAIL        = 'f'
} shapito_fe_msg_t;

typedef enum
{
	SHAPITO_BE_AUTHENTICATION             = 'R',
	SHAPITO_BE_BACKEND_KEY_DATA           = 'K',
	SHAPITO_BE_PARSE_COMPLETE             = '1',
	SHAPITO_BE_BIND_COMPLETE              = '2',
	SHAPITO_BE_CLOSE_COMPLETE             = '3',
	SHAPITO_BE_COMMAND_COMPLETE           = 'C',
	SHAPITO_BE_COPY_IN_RESPONSE           = 'G',
	SHAPITO_BE_COPY_OUT_RESPONSE          = 'H',
	SHAPITO_BE_COPY_BOTH_RESPONSE         = 'W',
	SHAPITO_BE_COPY_DATA                  = 'd',
	SHAPITO_BE_COPY_DONE                  = 'c',
	SHAPITO_BE_COPY_FAIL                  = 'f',
	SHAPITO_BE_DATA_ROW                   = 'D',
	SHAPITO_BE_EMPTY_QUERY_RESPONSE       = 'I',
	SHAPITO_BE_ERROR_RESPONSE             = 'E',
	SHAPITO_BE_FUNCTION_CALL_RESPONSE     = 'V',
	SHAPITO_BE_NEGOTIATE_PROTOCOL_VERSION = 'v',
	SHAPITO_BE_NO_DATA                    = 'n',
	SHAPITO_BE_NOTICE_RESPONSE            = 'N',
	SHAPITO_BE_NOTIFICATION_RESPONSE      = 'A',
	SHAPITO_BE_PARAMETER_DESCRIPTION      = 't',
	SHAPITO_BE_PARAMETER_STATUS           = 'S',
	SHAPITO_BE_PORTAL_SUSPENDED           = 's',
	SHAPITO_BE_READY_FOR_QUERY            = 'Z',
	SHAPITO_BE_ROW_DESCRIPTION            = 'T',
} shapito_be_msg_t;

#endif /* SHAPITO_MSG_H */
