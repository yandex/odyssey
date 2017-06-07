
PostgreSQL protocol 3.0 cheatsheet
----------------------------------

startup packet

	u32 len  # len including len
	data[]

header

	u8  type # readable char
	u32 len  # len including len
	data[]

Message flow
------------

STARTUP

	C: SSLRequest
	S: # one byte: 'S', 'N'
	S: 'S' # proceed with SSL
	S: 'N' # not supported
		- maybe close connection

	C: StartupMessage # or CancelRequest
	# { len, version: 0x30000, user...database...options..}

	S: # AUTH require message
	S: AuthenticationClearTextPassword # {'R', 8, 3 }
	C: PasswordMessage # { 'p', len, data }

	S: ErrorResponce # { 'E', len, 'S' ... ,  } 
		- close connection
	S: AuthenticationOk # {'R', 8, 0 }

	# new backend created; server tries to apply startup options
	S: ErrorResponce
		- close connection
	
	S: BackendKeyData # { 'K', 12, u32 pid, u32 key }
		- cancel key

	S: ParameterStatus # { 'S', len, name, value }
		- client_encoding, etc...

	S: NoticeResponce # { 'N', len, ... } same as error
		- just print the message
	
	S: ReadyForQuery # { 'Z', 5, tx }  tx: 'I' inactive, 'T' in tx, 'E' cancelled tx

TERMINATE

	C: Terminate # { 'F', 4 }
		- no wait, close connection

CANCEL

	# new connection required*
	# no reply from server*
	C: CancelRequest # { 16, 80877102, pid, key }

QUERY

	C: Query # { 'Q', len, text }

	S: RowDescription
	S: DataRow
	S: CommandComplete #  can be several

	#  can be repeated several time for reach stmt;stmt

	S: EmptyQueryResponce # in case if text=""
	S: NoticeResponce
	S: ErrorResponce

	# all text stmts processed, ready for next command
	S: ReadyForQuery

EXTENDED QUERY

	C: Parse # { 'P', len, operator_name, query, u16 types_count, ... u32 type }
             #                can be 0                 can be 0         can be 0
             #
             # Parse can have only one query, no ';'.
             #
             # Nameless operator exists till next query.
             #
             # Named operator exists till end of a session, or explicitly destroyed.
             # Named operator available for SQL Prepare and Execute calls.

	S: ParseComplete # { '1', len }
	S: ErrorResponce

	C: Bind # { 'B', len, portal_name, operator_name,
            #	   16  argc_call_arg_types_count, u16[] types,
            #                                   0 or 1
            #    u16 argc, [ i32 len,  bytes],
            #
            #    u16 argc_result_cols_types_count, u16[] types }
            #
            # Named/Nameless Portal exists till transaction ends
            #
            # Named portal avail for SQL Declare, Cursor, Fetch

	S: BindComplete # { '2', len }

	C: Describe # { 'D', len, u8 'S'/'P', operator_or_portal_name }
	C: Execute # { 'E', len, portal_name, u32 limit } 0 - limitless

	S: PortalSuspended # { 's', len }   on execute limit reach

	S: NoData  # { 'n', len } on Describe, when portal has no query
	S: RowDescription  # answer for Describe
	S: DataRow

	S: CommandComplete
	S: EmptyQueryResponce
	S: ErrorResponce # Skips all commands till Sync

	C: Close # { 'C', len, u8 'S'/'P', operator_or_portal }
	C: Sync  # { 'S', len }

	S: ReadyForQuery

FUNCTION CALL
COPY
ASYNC
