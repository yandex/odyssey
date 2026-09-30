#pragma once

#include <setjmp.h>
#include <sql/full/pg_list.h>
#include <sql/full/scanner.h>
#include <sql/full/parser.h>
#include <sql/full/pg_constants.h>

#include "gram.tab.h"

/* Error reporting stubs — gram.y uses ereport/errmsg/errcode/errhint */
#define ereport(level, ...) scanner_yyerror("syntax error", yyscanner)
#define elog(level, ...) scanner_yyerror("syntax error", yyscanner)
#define errmsg(...) ""
#define errmsg_internal(...) ""
#define errcode(x) 0
#define errhint(...)
#define errposition(x) 0
#define errcode_for_file_access() 0
#define ERRCODE_SYNTAX_ERROR 0
#define ERRCODE_INVALID_NAME 0
#define ERRCODE_INVALID_PARAMETER_VALUE 0
#define ERRCODE_WRONG_OBJECT_TYPE 0
#define ERRCODE_UNDEFINED_TABLE 0
#define ERRCODE_DUPLICATE_COLUMN 0
#define ERRCODE_UNDEFINED_COLUMN 0
#define ERRCODE_AMBIGUOUS_COLUMN 0
#define ERRCODE_UNDEFINED_OBJECT 0
#define ERRCODE_DUPLICATE_OBJECT 0
#define ERRCODE_INVALID_OBJECT_DEFINITION 0
#define ERRCODE_INVALID_ROW_DEFINITION 0
#define ERRCODE_DATATYPE_MISMATCH 0
#define ERRCODE_INVALID_TEXT_REPRESENTATION 0
#define WARNING 0
#define ERROR 0
#define NOTICE 0
#define INFO 0
#define DEBUG1 0
#define pg_log_level 0
#include <strings.h>
#define pg_strcasecmp strcasecmp
typedef int GroupingSetKind;
typedef int XmlExprOp;
typedef int SQLValueFunctionOp;
typedef int SetOperation;
typedef int PublicationObjSpecType;
typedef int PartitionStrategy;
typedef int RoleSpecType;
bool equal(const void *a, const void *b);
char *psprintf(const char *fmt, ...);
List *SystemFuncName(char *name);
TypeName *SystemTypeName(char *name);

/* Enum constants used in gram.y */
#define DROP_RESTRICT 0
#define DROP_CASCADE 1
#define ROLESTMT_ROLE 0
#define ROLESTMT_USER 1
#define ROLESTMT_GROUP 2
#define VAR_SET_VALUE 0
#define VAR_SET_DEFAULT 1
#define VAR_SET_CURRENT 2
#define VAR_SET_MULTI 3
#define VAR_RESET 4
#define VAR_RESET_ALL 5
#define VAR_SET 6
#define FRAMEOPTION_NONRANGE 0
#define FRAMEOPTION_RANGE 0x01
#define FRAMEOPTION_ROWS 0x02
#define FRAMEOPTION_GROUPS 0x04
#define FRAMEOPTION_BETWEEN 0x08
#define FRAMEOPTION_START_UNBOUNDED_PRECEDING 0x10
#define FRAMEOPTION_END_UNBOUNDED_PRECEDING 0x20
#define FRAMEOPTION_START_UNBOUNDED_FOLLOWING 0x40
#define FRAMEOPTION_END_UNBOUNDED_FOLLOWING 0x80
#define FRAMEOPTION_START_CURRENT_ROW 0x100
#define FRAMEOPTION_END_CURRENT_ROW 0x200
#define CURSOR_OPT_BINARY 0x01
#define CURSOR_OPT_SCROLL 0x02
#define CURSOR_OPT_NO_SCROLL 0x04
#define CURSOR_OPT_HOLD 0x08
#define CURSOR_OPT_FAST_PLAN 0x10
#define CURSOR_OPT_CUSTOM_PLAN 0x20
#define CURSOR_OPT_GENERIC_PLAN 0x40
#define CONSTR_NULL 0
#define CONSTR_NOTNULL 1
#define CONSTR_DEFAULT 2
#define CONSTR_IDENTITY 3
#define CONSTR_GENERATED 4
#define CONSTR_CHECK 5
#define CONSTR_PRIMARY 6
#define CONSTR_UNIQUE 7
#define CONSTR_EXCLUSION 8
#define CONSTR_FOREIGN 9
#define CONSTR_ATTR_DEFERRABLE 10
#define CONSTR_ATTR_NOT_DEFERRABLE 11
#define CONSTR_ATTR_INITIALLY_DEFERRED 12
#define CONSTR_ATTR_INITIALLY_IMMEDIATE 13
#define CONSTR_ATTR_NOT_VALID 14
#define CONSTR_ATTR_NO_INHERIT 15
#define XMLOPTION_DOCUMENT 0
#define XMLOPTION_CONTENT 1
#define FETCH_ALL -1
#define AT_ADD_COLUMN 1
#define AT_AlterConstraint 2
#define SETOP_NONE 0
#define SETOP_UNION 1
#define SETOP_INTERSECT 2
#define SETOP_EXCEPT 3

/* DiscardMode */
#define DISCARD_ALL 0
#define DISCARD_PLANS 1
#define DISCARD_SEQUENCES 2
#define DISCARD_TEMP 3

/* ObjectType */
#define OBJECT_TABLE 0
#define OBJECT_INDEX 1

/* Interval constants */
#define HOUR 1
#define MINUTE 2
#define INTERVAL_MASK(b) (1 << (b))
#define INTERVAL_FULL_RANGE 0xFFFFFFFF
#define INTERVAL_RANGE(b) (1 << (b))

#define OBJECT_SEQUENCE 2
#define OBJECT_VIEW 3
#define OBJECT_MATVIEW 4
#define OBJECT_FOREIGN_TABLE 5
#define OBJECT_COLUMN 6
#define OBJECT_DATABASE 7
#define OBJECT_FUNCTION 8
#define OBJECT_PROCEDURE 9
#define OBJECT_SCHEMA 10
#define OBJECT_TYPE 11
#define OBJECT_DOMAIN 12
#define OBJECT_AGGREGATE 13
#define OBJECT_OPERATOR 14
#define OBJECT_TRIGGER 15
#define OBJECT_CONSTRAINT 16
#define OBJECT_RULE 17
#define OBJECT_POLICY 18
#define OBJECT_EVENT_TRIGGER 19
#define OBJECT_EXTENSION 20
#define OBJECT_FDW 21
#define OBJECT_FOREIGN_SERVER 22
#define OBJECT_USER_MAPPING 23
#define OBJECT_OPCLASS 24
#define OBJECT_OPFAMILY 25
#define OBJECT_COLLATION 26
#define OBJECT_CONVERSION 27
#define OBJECT_CAST 28
#define OBJECT_LANGUAGE 29
#define OBJECT_LARGEOBJECT 30
#define OBJECT_PUBLICATION 31
#define OBJECT_PUBLICATION_REL 32
#define OBJECT_ROLE 33
#define OBJECT_SUBSCRIPTION 34
#define OBJECT_STATISTIC_EXT 35
#define OBJECT_TABCONSTRAINT 36
#define OBJECT_TRANSFORM 37
#define OBJECT_TSCONFIGURATION 38
#define OBJECT_TSDICTIONARY 39
#define OBJECT_TSPARSER 40
#define OBJECT_TSTEMPLATE 41
#define OBJECT_ACCESS_METHOD 42
#define OBJECT_AMOP 43
#define OBJECT_AMPROC 44
#define OBJECT_ATTRIBUTE 45
#define OBJECT_DEFAULT 46
#define OBJECT_DEFAULT_ACL 47
#define OBJECT_DEFACL 48
#define OBJECT_DOMCONSTRAINT 49
#define OBJECT_PARAMETER_ACL 53
#define OBJECT_TABLESPACE 54
#define OBJECT_TSDATA 55

typedef struct base_yy_extra_type
{
	core_yy_extra_type core_yy_extra;

	bool		have_lookahead;
	int			lookahead_token;
	core_YYSTYPE lookahead_yylval;
	YYLTYPE		lookahead_yylloc;
	char	   *lookahead_end;
	char		lookahead_hold_char;

	List	   *parsetree;

	/* error handling */
	jmp_buf		error_jmp;
	int			error_set;
	int			had_error;
	od_sql_full_error_cb_t error_cb;
	void	   *error_cb_userdata;
} base_yy_extra_type;

#define pg_yyget_extra(yyscanner) (*((base_yy_extra_type **) (yyscanner)))

extern int	base_yylex(BASE_YYSTYPE *lvalp, YYLTYPE *llocp,
					   core_yyscan_t yyscanner);

extern void parser_init(base_yy_extra_type *yyext);
extern int	base_yyparse(core_yyscan_t yyscanner);


#define AT_AddColumn 1
#define AT_ColumnDefault 2
#define AT_DropNotNull 3
#define AT_SetNotNull 4
#define AT_SetStatistics 5
#define AT_SetOptions 6
#define AT_DropExpression 7
#define AT_SetExpression 8
#define PG_INT16_MAX 32767
#define AT_ResetOptions 12
#define AT_SetStorage 13
#define AT_SetCompression 14
#define AT_AddConstraint 15
#define AT_DropConstraint 16
#define AT_ValidateConstraint 17
#define AT_AlterColumnType 21
#define AT_DropColumn 22
#define AT_AlterColumnGenericOptions 23
/* AlterTableType - remaining constants */
#define AT_DropOids 24
#define AT_ClusterOn 25
#define AT_DropCluster 26
#define AT_SetLogged 27
#define AT_SetUnLogged 28
#define AT_EnableTrig 29
#define AT_EnableAlwaysTrig 30
#define AT_EnableReplicaTrig 31
#define AT_DisableTrig 32
#define AT_EnableTrigAll 33
#define AT_DisableTrigAll 34
#define AT_EnableTrigUser 35
#define AT_DisableTrigUser 36
#define AT_EnableRule 37
#define AT_EnableAlwaysRule 38
#define AT_EnableReplicaRule 39
#define AT_DisableRule 40
#define AT_AddInherit 41
#define AT_DropInherit 42
#define AT_AddOf 43
#define AT_DropOf 44
#define AT_ChangeOwner 45
#define AT_SetAccessMethod 46
#define AT_SetTableSpace 47
#define AT_SetRelOptions 48
#define AT_ResetRelOptions 49
#define AT_ReplicaIdentity 50
#define AT_EnableRowSecurity 51
#define AT_DisableRowSecurity 52
#define AT_ForceRowSecurity 53
#define AT_NoForceRowSecurity 54
#define AT_GenericOptions 55
#define AT_AttachPartition 56
#define AT_DetachPartition 57
#define AT_DetachPartitionFinalize 58
#define AT_AddIdentity 59
#define AT_DropIdentity 60
#define AT_SetIdentity 61
#define REPLICA_IDENTITY_NOTHING 0
#define REPLICA_IDENTITY_DEFAULT 1
#define REPLICA_IDENTITY_USING_INDEX 2
#define REPLICA_IDENTITY_FULL 3

#define REPLICA_IDENTITY_INDEX 4
#define DEFELEM_UNSPEC 0
#define DEFELEM_SET 1
#define DEFELEM_ADD 2
#define DEFELEM_DROP 3
#define PARTITION_STRATEGY_HASH 'h'
#define PARTITION_STRATEGY_LIST 'l'
#define PARTITION_STRATEGY_RANGE 'r'
#define RELPERSISTENCE_TEMP 't'
#define RELPERSISTENCE_UNLOGGED 'u'
#define RELPERSISTENCE_PERMANENT 'p'
#define InvalidOid 0

/* Helper function */
static inline int defGetInt32(DefElem *def)
{
    (void)def;
    return 0;
}
#define ATTRIBUTE_IDENTITY_ALWAYS 'a'
#define ATTRIBUTE_IDENTITY_BY_DEFAULT 'd'
#define ATTRIBUTE_GENERATED_STORED 's'
#define ATTRIBUTE_GENERATED_VIRTUAL 'v'
#define CONSTR_ATTR_DEFERRED 16
#define CONSTR_ATTR_IMMEDIATE 17
#define CONSTR_ATTR_ENFORCED 18
#define CONSTR_ATTR_NOT_ENFORCED 19
#define CREATE_TABLE_LIKE_DEFAULTS 0x01
#define CREATE_TABLE_LIKE_CONSTRAINTS 0x02
#define CREATE_TABLE_LIKE_IDENTITY 0x04
#define CREATE_TABLE_LIKE_GENERATED 0x08
#define CREATE_TABLE_LIKE_COMMENTS 0x10
#define CREATE_TABLE_LIKE_COMPRESSION 0x20
#define CREATE_TABLE_LIKE_ALL 0x7FFFFFFF
#define CREATE_TABLE_LIKE_INDEXES 0x40
#define CREATE_TABLE_LIKE_STATISTICS 0x80
#define CREATE_TABLE_LIKE_STORAGE 0x100
#define FKCONSTR_MATCH_FULL 'f'
#define FKCONSTR_MATCH_PARTIAL 'p'
#define FKCONSTR_MATCH_SIMPLE 's'
#define FKCONSTR_ACTION_NOACTION 'a'
#define FKCONSTR_ACTION_RESTRICT 'r'
#define FKCONSTR_ACTION_CASCADE 'c'
#define FKCONSTR_ACTION_SETNULL 'n'
#define FKCONSTR_ACTION_SETDEFAULT 'd'
#define ONCOMMIT_NOOP 0
#define ONCOMMIT_PRESERVE_ROWS 1
#define ONCOMMIT_DELETE_ROWS 2
#define ONCOMMIT_DROP 3
#define OBJECT_ROUTINE 57

Expr *makeBoolExpr(int boolop, List *args, ParseLoc location);

void *copyObject(const void *from);
