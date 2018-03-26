
/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

#include "shapito.h"

static char *shapito_fe_msgs[] =
{
	[SHAPITO_FE_TERMINATE]        = "Terminate",
	[SHAPITO_FE_PASSWORD_MESSAGE] = "PasswordMessage",
	[SHAPITO_FE_QUERY]            = "Query",
	[SHAPITO_FE_FUNCTION_CALL]    = "FunctionCall",
	[SHAPITO_FE_PARSE]            = "Parse",
	[SHAPITO_FE_BIND]             = "Bind",
	[SHAPITO_FE_DESCRIBE]         = "Describe",
	[SHAPITO_FE_EXECUTE]          = "Execute",
	[SHAPITO_FE_SYNC]             = "Sync",
	[SHAPITO_FE_CLOSE]            = "Close",
	[SHAPITO_FE_COPY_DATA]        = "CopyData",
	[SHAPITO_FE_COPY_DONE]        = "CopyDone",
	[SHAPITO_FE_COPY_FAIL]        = "CopyFail"
};

static char *shapito_be_msgs[] =
{
	[SHAPITO_BE_AUTHENTICATION]             = "Authentication",
	[SHAPITO_BE_BACKEND_KEY_DATA]           = "BackendKeyData",
	[SHAPITO_BE_PARSE_COMPLETE]             = "ParseComplete",
	[SHAPITO_BE_BIND_COMPLETE]              = "BindComplete",
	[SHAPITO_BE_CLOSE_COMPLETE]             = "CloseComplete",
	[SHAPITO_BE_COMMAND_COMPLETE]           = "CommandComplete",
	[SHAPITO_BE_COPY_IN_RESPONSE]           = "CopyInResponse",
	[SHAPITO_BE_COPY_OUT_RESPONSE]          = "CopyOutResponse",
	[SHAPITO_BE_COPY_BOTH_RESPONSE]         = "CopyBothResponse",
	[SHAPITO_BE_COPY_DATA]                  = "CopyData",
	[SHAPITO_BE_COPY_DONE]                  = "CopyDone",
	[SHAPITO_BE_COPY_FAIL]                  = "CopyFail",
	[SHAPITO_BE_DATA_ROW]                   = "DataRow",
	[SHAPITO_BE_EMPTY_QUERY_RESPONSE]       = "EmptyQueryResponse",
	[SHAPITO_BE_ERROR_RESPONSE]             = "ErrorResponse",
	[SHAPITO_BE_FUNCTION_CALL_RESPONSE]     = "FunctionCallResponse",
	[SHAPITO_BE_NEGOTIATE_PROTOCOL_VERSION] = "NegotiateProtocolVersion",
	[SHAPITO_BE_NO_DATA]                    = "NoData",
	[SHAPITO_BE_NOTICE_RESPONSE]            = "NoticeResponse",
	[SHAPITO_BE_NOTIFICATION_RESPONSE]      = "NotificationResponse",
	[SHAPITO_BE_PARAMETER_DESCRIPTION]      = "ParameterDescription",
	[SHAPITO_BE_PARAMETER_STATUS]           = "ParameterStatus",
	[SHAPITO_BE_PORTAL_SUSPENDED]           = "PortalSuspended",
	[SHAPITO_BE_READY_FOR_QUERY]            = "ReadyForQuery",
	[SHAPITO_BE_ROW_DESCRIPTION]            = "RowDescription"
};

char*
shapito_fe_msg_to_string(int type)
{
	char *desc = shapito_fe_msgs[type];
	if (desc == NULL)
		desc = "Unknown";
	return desc;
}

char*
shapito_be_msg_to_string(int type)
{
	char *desc = shapito_be_msgs[type];
	if (desc == NULL)
		desc = "Unknown";
	return desc;
}
