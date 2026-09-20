#pragma once

#include <sql/full/nodes.h>
#include <sql/full/pg_list.h>

/* Value types */
typedef struct Integer
{
NodeTag		type;
	int			ival;
} Integer;

typedef struct Float
{
NodeTag		type;
	char	   *fval;
} Float;

typedef struct Boolean
{
NodeTag		type;
	bool		boolval;
} Boolean;

typedef struct String
{
NodeTag		type;
	char	   *sval;
} String;

typedef struct BitString
{
NodeTag		type;
	char	   *bsval;
} BitString;

typedef struct Null
{
NodeTag		type;
} Null;

typedef union ValUnion
{
	Node		node;
	Integer		ival;
	Float		fval;
	Boolean		boolval;
	String		sval;
	BitString	bsval;
} ValUnion;

typedef struct Node Expr;
typedef struct Node FuncExpr;

/* Forward declarations */
typedef struct A_Expr A_Expr;

typedef struct A_Expr
{
	NodeTag		type;
	int		kind;
	List	   *name;
	Node	   *lexpr;
	Node	   *rexpr;
	ParseLoc	rexpr_list_start;
	ParseLoc	rexpr_list_end;
	ParseLoc	location;
} A_Expr;

typedef struct JsonFormat
{
	NodeTag		type;
	int		format_type;
	int		encoding;
	ParseLoc	location;
} JsonFormat;

typedef struct JsonValueExpr
{
	NodeTag		type;
	Node	   *raw_expr;
	Node	   *formatted_expr;
	JsonFormat *format;
} JsonValueExpr;

typedef struct JsonTablePathSpec
{
	NodeTag		type;
	char	   *string;
	char	   *name;
	ParseLoc	location;
} JsonTablePathSpec;

typedef struct JsonTablePath
{
	NodeTag		type;
	Node	   *pathvalue;
	char	   *pathname;
} JsonTablePath;

typedef struct JsonBehavior
{
	NodeTag		type;
	int		btype;
	Node	   *expr;
	ParseLoc	location;
} JsonBehavior;


typedef struct GroupingSet GroupingSet;

typedef struct GroupingSet
{
	NodeTag		type;
	int		kind;
	List	   *content;
	ParseLoc	location;
} GroupingSet;

typedef struct ATAlterConstraint ATAlterConstraint;
typedef struct A_ArrayExpr A_ArrayExpr;
typedef struct A_Const A_Const;
typedef struct A_Indices A_Indices;
typedef struct A_Indirection A_Indirection;
typedef struct A_Star A_Star;
typedef struct AccessPriv AccessPriv;
typedef struct Alias Alias;
typedef struct AlterCollationStmt AlterCollationStmt;
typedef struct AlterDatabaseRefreshCollStmt AlterDatabaseRefreshCollStmt;
typedef struct AlterDatabaseSetStmt AlterDatabaseSetStmt;
typedef struct AlterDatabaseStmt AlterDatabaseStmt;
typedef struct AlterDefaultPrivilegesStmt AlterDefaultPrivilegesStmt;
typedef struct AlterDomainStmt AlterDomainStmt;
typedef struct AlterEnumStmt AlterEnumStmt;
typedef struct AlterEventTrigStmt AlterEventTrigStmt;
typedef struct AlterExtensionContentsStmt AlterExtensionContentsStmt;
typedef struct AlterExtensionStmt AlterExtensionStmt;
typedef struct AlterFdwStmt AlterFdwStmt;
typedef struct AlterForeignServerStmt AlterForeignServerStmt;
typedef struct AlterFunctionStmt AlterFunctionStmt;
typedef struct AlterObjectDependsStmt AlterObjectDependsStmt;
typedef struct AlterObjectSchemaStmt AlterObjectSchemaStmt;
typedef struct AlterOpFamilyStmt AlterOpFamilyStmt;
typedef struct AlterOperatorStmt AlterOperatorStmt;
typedef struct AlterOwnerStmt AlterOwnerStmt;
typedef struct AlterPolicyStmt AlterPolicyStmt;
typedef struct AlterPublicationStmt AlterPublicationStmt;
typedef struct AlterRoleSetStmt AlterRoleSetStmt;
typedef struct AlterRoleStmt AlterRoleStmt;
typedef struct AlterSeqStmt AlterSeqStmt;
typedef struct AlterStatsStmt AlterStatsStmt;
typedef struct AlterSubscriptionStmt AlterSubscriptionStmt;
typedef struct AlterSystemStmt AlterSystemStmt;
typedef struct AlterTSConfigurationStmt AlterTSConfigurationStmt;
typedef struct AlterTSDictionaryStmt AlterTSDictionaryStmt;
typedef struct AlterTableCmd AlterTableCmd;
typedef struct AlterTableMoveAllStmt AlterTableMoveAllStmt;
typedef struct AlterTableSpaceOptionsStmt AlterTableSpaceOptionsStmt;
typedef struct AlterTableStmt AlterTableStmt;
typedef struct AlterTypeStmt AlterTypeStmt;
typedef struct AlterUserMappingStmt AlterUserMappingStmt;
typedef struct BoolExpr BoolExpr;
typedef struct BooleanTest BooleanTest;
typedef struct CTECycleClause CTECycleClause;
typedef struct CTESearchClause CTESearchClause;
typedef struct CallStmt CallStmt;
typedef struct CaseExpr CaseExpr;
typedef struct CaseWhen CaseWhen;
typedef struct CheckPointStmt CheckPointStmt;
typedef struct ClosePortalStmt ClosePortalStmt;
typedef struct CoalesceExpr CoalesceExpr;
typedef struct CollateClause CollateClause;
typedef struct ColumnDef ColumnDef;
typedef struct ColumnRef ColumnRef;
typedef struct CommentStmt CommentStmt;
typedef struct CommonTableExpr CommonTableExpr;
typedef struct CompositeTypeStmt CompositeTypeStmt;
typedef struct ConstraintsSetStmt ConstraintsSetStmt;
typedef struct CopyStmt CopyStmt;
typedef struct CreateAmStmt CreateAmStmt;
typedef struct CreateCastStmt CreateCastStmt;
typedef struct CreateConversionStmt CreateConversionStmt;
typedef struct CreateDomainStmt CreateDomainStmt;
typedef struct CreateEnumStmt CreateEnumStmt;
typedef struct CreateEventTrigStmt CreateEventTrigStmt;
typedef struct CreateExtensionStmt CreateExtensionStmt;
typedef struct CreateFdwStmt CreateFdwStmt;
typedef struct CreateForeignServerStmt CreateForeignServerStmt;
typedef struct RangeVar RangeVar;
typedef struct TypeName TypeName;
typedef struct CreateStmt
{
	NodeTag		type;
	RangeVar   *relation;
	List	   *tableElts;
	List	   *inhRelations;
	Node	   *partbound;
	Node	   *partspec;
	TypeName   *ofTypename;
	List	   *constraints;
	List	   *options;
	char	   *accessMethod;
	char	   *tablespacename;
	int		oncommit;
	bool		if_not_exists;
} CreateStmt;

typedef struct CreateForeignTableStmt CreateForeignTableStmt;
typedef struct CreateFunctionStmt CreateFunctionStmt;
typedef struct CreateOpClassItem CreateOpClassItem;
typedef struct CreateOpClassStmt CreateOpClassStmt;
typedef struct CreateOpFamilyStmt CreateOpFamilyStmt;
typedef struct CreatePLangStmt CreatePLangStmt;
typedef struct CreatePolicyStmt CreatePolicyStmt;
typedef struct CreatePublicationStmt CreatePublicationStmt;
typedef struct CreateRangeStmt CreateRangeStmt;
typedef struct CreateRoleStmt CreateRoleStmt;
typedef struct CreateSchemaStmt CreateSchemaStmt;
typedef struct CreateSeqStmt CreateSeqStmt;
typedef struct CreateStatsStmt CreateStatsStmt;
typedef struct CreateStmt CreateStmt;
typedef struct CreateSubscriptionStmt CreateSubscriptionStmt;
typedef struct CreateTableAsStmt CreateTableAsStmt;
typedef struct CreateTableSpaceStmt CreateTableSpaceStmt;
typedef struct CreateTransformStmt CreateTransformStmt;
typedef struct CreateTrigStmt CreateTrigStmt;
typedef struct CreateUserMappingStmt CreateUserMappingStmt;
typedef struct CreatedbStmt CreatedbStmt;
typedef struct CurrentOfExpr CurrentOfExpr;
typedef struct DeallocateStmt DeallocateStmt;
typedef struct DeclareCursorStmt DeclareCursorStmt;
typedef struct DefineStmt DefineStmt;
typedef struct DeleteStmt DeleteStmt;
typedef struct DiscardStmt DiscardStmt;
typedef struct DoStmt DoStmt;
typedef struct DropOwnedStmt DropOwnedStmt;
typedef struct DropRoleStmt DropRoleStmt;
typedef struct DropStmt DropStmt;
typedef struct DropSubscriptionStmt DropSubscriptionStmt;
typedef struct DropTableSpaceStmt DropTableSpaceStmt;
typedef struct DropUserMappingStmt DropUserMappingStmt;
typedef struct DropdbStmt DropdbStmt;
typedef struct ExecuteStmt ExecuteStmt;
typedef struct ExplainStmt ExplainStmt;
typedef struct Node Expr;
typedef struct FetchStmt FetchStmt;
typedef struct FuncCall FuncCall;
typedef struct Node FuncExpr;
typedef struct FunctionParameter FunctionParameter;
typedef struct GrantRoleStmt GrantRoleStmt;
typedef struct GrantStmt GrantStmt;
typedef struct GroupingFunc GroupingFunc;
typedef struct ImportForeignSchemaStmt ImportForeignSchemaStmt;
typedef struct IndexElem IndexElem;
typedef struct IndexStmt IndexStmt;
typedef struct InferClause InferClause;
typedef struct InsertStmt InsertStmt;
typedef struct IntoClause IntoClause;
typedef struct JoinExpr JoinExpr;
typedef struct JsonAggConstructor JsonAggConstructor;
typedef struct JsonArgument JsonArgument;
typedef struct JsonArrayAgg JsonArrayAgg;
typedef struct JsonArrayConstructor JsonArrayConstructor;
typedef struct JsonArrayQueryConstructor JsonArrayQueryConstructor;
typedef struct JsonFormat JsonFormat;
typedef struct JsonFuncExpr JsonFuncExpr;
typedef struct JsonObjectAgg JsonObjectAgg;
typedef struct JsonObjectConstructor JsonObjectConstructor;
typedef struct JsonOutput JsonOutput;
typedef struct JsonParseExpr JsonParseExpr;
typedef struct JsonReturning JsonReturning;
typedef struct JsonScalarExpr JsonScalarExpr;
typedef struct JsonSerializeExpr JsonSerializeExpr;
typedef struct JsonTable JsonTable;
typedef struct JsonTableColumn JsonTableColumn;
typedef struct JsonTablePlanSpec JsonTablePlanSpec;
typedef struct ListenStmt ListenStmt;
typedef struct LoadStmt LoadStmt;
typedef struct LockStmt LockStmt;
typedef struct LockingClause LockingClause;
typedef struct MergeStmt MergeStmt;
typedef struct MergeSupportFunc MergeSupportFunc;
typedef struct MergeWhenClause MergeWhenClause;
typedef struct MinMaxExpr MinMaxExpr;
typedef struct MultiAssignRef MultiAssignRef;
typedef struct NamedArgExpr NamedArgExpr;
typedef struct NotifyStmt NotifyStmt;
typedef struct NullTest NullTest;
typedef struct ObjectWithArgs ObjectWithArgs;
typedef struct OnConflictClause OnConflictClause;
typedef struct PLAssignStmt PLAssignStmt;
typedef struct ParamRef ParamRef;
typedef struct PartitionCmd PartitionCmd;
typedef struct PartitionElem PartitionElem;
typedef struct PartitionBoundSpec PartitionBoundSpec;
typedef struct PartitionBoundSpec
{
	NodeTag		type;
	char		strategy;
	bool		is_default;
	int		modulus;
	int		remainder;
	List	   *listdatums;
	List	   *lowerdatums;
	List	   *upperdatums;
	ParseLoc	location;
} PartitionBoundSpec;
typedef struct PartitionSpec PartitionSpec;
typedef struct PrepareStmt PrepareStmt;
typedef struct PublicationAllObjSpec PublicationAllObjSpec;
typedef struct PublicationObjSpec PublicationObjSpec;
typedef struct PublicationTable PublicationTable;
typedef struct RangeFunction RangeFunction;
typedef struct RangeSubselect RangeSubselect;
typedef struct RangeTableFunc RangeTableFunc;
typedef struct RangeTableFuncCol RangeTableFuncCol;
typedef struct RangeTableSample RangeTableSample;
typedef struct RangeVar RangeVar;
typedef struct RawStmt RawStmt;
typedef struct ReassignOwnedStmt ReassignOwnedStmt;
typedef struct RefreshMatViewStmt RefreshMatViewStmt;
typedef struct ReindexStmt ReindexStmt;
typedef struct RenameStmt RenameStmt;
typedef struct RepackStmt RepackStmt;
typedef struct ReplicaIdentityStmt ReplicaIdentityStmt;
typedef struct ResTarget ResTarget;
typedef struct ReturnStmt ReturnStmt;
typedef struct ReturningClause ReturningClause;
typedef struct ReturningOption ReturningOption;
typedef struct RoleSpec RoleSpec;
typedef struct RowExpr RowExpr;
typedef struct RuleStmt RuleStmt;
typedef struct SQLValueFunction SQLValueFunction;
typedef struct SecLabelStmt SecLabelStmt;
typedef struct SelectStmt SelectStmt;
typedef struct SetToDefault SetToDefault;
typedef struct SortBy SortBy;
typedef struct StatsElem StatsElem;
typedef struct SubLink SubLink;
typedef struct TableLikeClause TableLikeClause;
typedef struct TransactionStmt TransactionStmt;
typedef struct TriggerTransition TriggerTransition;
typedef struct TruncateStmt TruncateStmt;
typedef struct TypeCast TypeCast;
typedef struct UnlistenStmt UnlistenStmt;
typedef struct UpdateStmt UpdateStmt;
typedef struct VacuumRelation VacuumRelation;
typedef struct VacuumStmt VacuumStmt;
typedef struct VariableSetStmt VariableSetStmt;
typedef struct VariableShowStmt VariableShowStmt;
typedef struct ViewStmt ViewStmt;
typedef struct WaitStmt WaitStmt;
typedef struct WindowDef WindowDef;
typedef struct WithClause WithClause;
typedef struct XmlExpr XmlExpr;
typedef struct XmlSerialize XmlSerialize;typedef struct TypeName TypeName;

typedef struct TypeName
{
	NodeTag		type;
	List	   *names;
	int		typeOid;
	bool		setof;
	bool		pct_type;
	List	   *typmods;
	int32		typemod;
	List	   *arrayBounds;
	ParseLoc	location;
} TypeName;typedef struct JsonValueExpr JsonValueExpr;
typedef struct JsonBehavior JsonBehavior;
typedef struct JsonTablePathSpec JsonTablePathSpec;
typedef struct JsonKeyValue JsonKeyValue;
typedef struct JsonIsPredicate JsonIsPredicate;typedef struct DefElem DefElem;

typedef struct DefElem
{
	NodeTag		type;
	char	   *defnamespace;
	char	   *defname;
	Node	   *arg;
	int		defaction;
	ParseLoc	location;
} DefElem;




typedef struct FuncCall FuncCall;
typedef FuncCall FunctionCall;
typedef struct BoolExpr BoolExpr;
typedef BoolExpr BooleanExpr;
typedef struct A_Indirection A_Indirection;
typedef A_Indirection Indirection;

#define strVal(v)		(castNode(String, v)->sval)
#define intVal(v)		(castNode(Integer, v)->ival)
#define boolVal(v)		(castNode(Boolean, v)->boolval)
#define floatVal(v)		(castNode(Float, v)->fval)

typedef struct ATAlterConstraint
{
NodeTag		type;
	char	   *conname;		
	bool		alterEnforceability;	
	bool		is_enforced;	
	bool		alterDeferrability; 
	bool		deferrable;		
	bool		initdeferred;	
	bool		alterInheritability;	
	bool		noinherit;
} ATAlterConstraint;

typedef struct A_ArrayExpr
{
NodeTag		type;
	List	   *elements;		
	ParseLoc	list_start;		
	ParseLoc	list_end;		
	ParseLoc	location;
} A_ArrayExpr;

typedef struct A_Const
{
NodeTag		type;
	union ValUnion val;
	bool		isnull;			
	ParseLoc	location;
} A_Const;

typedef struct A_Indices
{
NodeTag		type;
	bool		is_slice;		
	Node	   *lidx;			
	Node	   *uidx;
} A_Indices;

typedef struct A_Indirection
{
NodeTag		type;
	Node	   *arg;			
	List	   *indirection;
} A_Indirection;

typedef struct A_Star
{
NodeTag		type;
} A_Star;

typedef struct AccessPriv
{
NodeTag		type;
	char	   *priv_name;		
	List	   *cols;
} AccessPriv;

typedef struct Alias
{
NodeTag		type;
	char	   *aliasname;		
	List	   *colnames;
} Alias;

typedef struct AlterCollationStmt
{
NodeTag		type;
	List	   *collname;
} AlterCollationStmt;

typedef struct AlterDatabaseRefreshCollStmt
{
NodeTag		type;
	char	   *dbname;
} AlterDatabaseRefreshCollStmt;

typedef struct AlterDatabaseSetStmt
{
NodeTag		type;
	char	   *dbname;			
	VariableSetStmt *setstmt;
} AlterDatabaseSetStmt;

typedef struct AlterDatabaseStmt
{
NodeTag		type;
	char	   *dbname;			
	List	   *options;
} AlterDatabaseStmt;

typedef struct AlterDefaultPrivilegesStmt
{
NodeTag		type;
	List	   *options;		
	GrantStmt  *action;
} AlterDefaultPrivilegesStmt;

typedef struct AlterDomainStmt
{
NodeTag		type;
	int subtype;	
	List	   *typeName;		
	char	   *name;			
	Node	   *def;			
	int behavior;		
	bool		missing_ok;
} AlterDomainStmt;

typedef struct AlterEnumStmt
{
NodeTag		type;
	List	   *typeName;		
	char	   *oldVal;			
	char	   *newVal;			
	char	   *newValNeighbor; 
	bool		newValIsAfter;	
	bool		skipIfNewValExists;
} AlterEnumStmt;

typedef struct AlterEventTrigStmt
{
NodeTag		type;
	char	   *trigname;		
	char		tgenabled;
} AlterEventTrigStmt;

typedef struct AlterExtensionContentsStmt
{
NodeTag		type;
	char	   *extname;		
	int			action;			
	int	objtype;		
	Node	   *object;
} AlterExtensionContentsStmt;

typedef struct AlterExtensionStmt
{
NodeTag		type;
	char	   *extname;
	List	   *options;
} AlterExtensionStmt;

typedef struct AlterFdwStmt
{
NodeTag		type;
	char	   *fdwname;		
	List	   *func_options;	
	List	   *options;
} AlterFdwStmt;

typedef struct AlterForeignServerStmt
{
NodeTag		type;
	char	   *servername;		
	char	   *version;		
	List	   *options;		
	bool		has_version;
} AlterForeignServerStmt;

typedef struct AlterFunctionStmt
{
NodeTag		type;
	int	objtype;
	ObjectWithArgs *func;		
	List	   *actions;
} AlterFunctionStmt;

typedef struct AlterObjectDependsStmt
{
NodeTag		type;
	int	objectType;		
	RangeVar   *relation;		
	Node	   *object;			
	String	   *extname;		
	bool		remove;
} AlterObjectDependsStmt;

typedef struct AlterObjectSchemaStmt
{
NodeTag		type;
	int	objectType;		
	RangeVar   *relation;		
	Node	   *object;			
	char	   *newschema;		
	bool		missing_ok;
} AlterObjectSchemaStmt;

typedef struct AlterOpFamilyStmt
{
NodeTag		type;
	List	   *opfamilyname;	
	char	   *amname;			
	bool		isDrop;			
	List	   *items;
} AlterOpFamilyStmt;

typedef struct AlterOperatorStmt
{
NodeTag		type;
	ObjectWithArgs *opername;	
	List	   *options;
} AlterOperatorStmt;

typedef struct AlterOwnerStmt
{
NodeTag		type;
	int	objectType;		
	RangeVar   *relation;		
	Node	   *object;			
	RoleSpec   *newowner;
} AlterOwnerStmt;

typedef struct AlterPolicyStmt
{
NodeTag		type;
	char	   *policy_name;	
	RangeVar   *table;			
	List	   *roles;			
	Node	   *qual;			
	Node	   *with_check;
} AlterPolicyStmt;

typedef struct AlterPublicationStmt
{
NodeTag		type;
	char	   *pubname;		
	List	   *options;		
	List	   *pubobjects;		
	int action;	
	bool		for_all_tables; 
	bool		for_all_sequences;
} AlterPublicationStmt;

typedef struct AlterRoleSetStmt
{
NodeTag		type;
	RoleSpec   *role;			
	char	   *database;		
	VariableSetStmt *setstmt;
} AlterRoleSetStmt;

typedef struct AlterRoleStmt
{
NodeTag		type;
	RoleSpec   *role;			
	List	   *options;		
	int			action;
} AlterRoleStmt;

typedef struct AlterSeqStmt
{
NodeTag		type;
	RangeVar   *sequence;		
	List	   *options;
	bool		for_identity;
	bool		missing_ok;
} AlterSeqStmt;

typedef struct AlterStatsStmt
{
NodeTag		type;
	List	   *defnames;		
	Node	   *stxstattarget;	
	bool		missing_ok;
} AlterStatsStmt;

typedef struct AlterSubscriptionStmt
{
NodeTag		type;
	int kind; 
	char	   *subname;		
	char	   *servername;		
	char	   *conninfo;		
	List	   *publication;	
	List	   *options;
} AlterSubscriptionStmt;

typedef struct AlterSystemStmt
{
NodeTag		type;
	VariableSetStmt *setstmt;
} AlterSystemStmt;

typedef struct AlterTSConfigurationStmt
{
NodeTag		type;
	int kind;		
	List	   *cfgname;		
	List	   *tokentype;		
	List	   *dicts;			
	bool		override;		
	bool		replace;		
	bool		missing_ok;
} AlterTSConfigurationStmt;

typedef struct AlterTSDictionaryStmt
{
NodeTag		type;
	List	   *dictname;		
	List	   *options;
} AlterTSDictionaryStmt;

typedef struct AlterTableCmd
{
NodeTag		type;
	int subtype;		
	char	   *name;			
	int16		num;			
	RoleSpec   *newowner;
	Node	   *def;			
	int behavior;		
	bool		missing_ok;		
	bool		recurse;
} AlterTableCmd;

typedef struct AlterTableMoveAllStmt
{
NodeTag		type;
	char	   *orig_tablespacename;
	int	objtype;		
	List	   *roles;			
	char	   *new_tablespacename;
	bool		nowait;
} AlterTableMoveAllStmt;

typedef struct AlterTableSpaceOptionsStmt
{
NodeTag		type;
	char	   *tablespacename;
	List	   *options;
	bool		isReset;
} AlterTableSpaceOptionsStmt;

typedef struct AlterTableStmt
{
NodeTag		type;
	RangeVar   *relation;		
	List	   *cmds;			
	int	objtype;		
	bool		missing_ok;
} AlterTableStmt;

typedef struct AlterTypeStmt
{
NodeTag		type;
	List	   *typeName;		
	List	   *options;
} AlterTypeStmt;

typedef struct AlterUserMappingStmt
{
NodeTag		type;
	RoleSpec   *user;			
	char	   *servername;		
	List	   *options;
} AlterUserMappingStmt;

typedef struct BoolExpr
{
Expr		xpr;
	int boolop;
	List	   *args;			
	ParseLoc	location;
} BoolExpr;

typedef struct BooleanTest
{
Expr		xpr;
	Expr	   *arg;			
	int booltesttype;	
	ParseLoc	location;
} BooleanTest;

typedef struct CTECycleClause
{
NodeTag		type;
	List	   *cycle_col_list;
	char	   *cycle_mark_column;
	Node	   *cycle_mark_value;
	Node	   *cycle_mark_default;
	char	   *cycle_path_column;
	ParseLoc	location;
	int			cycle_mark_type;	
	int			cycle_mark_typmod;
	int			cycle_mark_collation;
	int			cycle_mark_neop;
} CTECycleClause;

typedef struct CTESearchClause
{
NodeTag		type;
	List	   *search_col_list;
	bool		search_breadth_first;
	char	   *search_seq_column;
	ParseLoc	location;
} CTESearchClause;

typedef struct CallStmt
{
NodeTag		type;
	FuncCall   *funccall ;
	FuncExpr   *funcexpr;
	List	   *outargs;
} CallStmt;

typedef struct CaseExpr
{
Expr		xpr;
	int			casetype ;
	int			casecollid ;
	Expr	   *arg;			
	List	   *args;			
	Expr	   *defresult;		
	ParseLoc	location;
} CaseExpr;

typedef struct CaseWhen
{
Expr		xpr;
	Expr	   *expr;			
	Expr	   *result;			
	ParseLoc	location;
} CaseWhen;

typedef struct CheckPointStmt
{
NodeTag		type;
	List	   *options;
} CheckPointStmt;

typedef struct ClosePortalStmt
{
NodeTag		type;
	char	   *portalname;
} ClosePortalStmt;

typedef struct CoalesceExpr
{
Expr		xpr;
	int			coalescetype ;
	int			coalescecollid ;
	List	   *args;
	ParseLoc	location;
} CoalesceExpr;

typedef struct CollateClause
{
NodeTag		type;
	Node	   *arg;			
	List	   *collname;		
	ParseLoc	location;
} CollateClause;

typedef struct ColumnDef
{
NodeTag		type;
	char	   *colname;		
	TypeName   *typeName;		
	char	   *compression;	
	int16		inhcount;		
	bool		is_local;		
	bool		is_not_null;	
	bool		is_from_type;	
	char		storage;		
	char	   *storage_name;	
	Node	   *raw_default;	
	Node	   *cooked_default; 
	char		identity;		
	RangeVar   *identitySequence;	
	char		generated;		
	CollateClause *collClause;	
	int			collOid;		
	List	   *constraints;	
	List	   *fdwoptions;		
	ParseLoc	location;
} ColumnDef;

typedef struct ColumnRef
{
NodeTag		type;
	List	   *fields;			
	ParseLoc	location;
} ColumnRef;

typedef struct CommentStmt
{
NodeTag		type;
	int	objtype;		
	Node	   *object;			
	char	   *comment;
} CommentStmt;

typedef struct CommonTableExpr
{
NodeTag		type;
	char	   *ctename;
	List	   *aliascolnames ;
	int ctematerialized; 
	Node	   *ctequery;		
	CTESearchClause *search_clause ;
	CTECycleClause *cycle_clause ;
	ParseLoc	location;		
	bool		cterecursive ;
	int			cterefcount ;
	List	   *ctecolnames ;
	List	   *ctecoltypes ;
	List	   *ctecoltypmods ;
	List	   *ctecolcollations ;
} CommonTableExpr;

typedef struct CompositeTypeStmt
{
NodeTag		type;
	RangeVar   *typevar;		
	List	   *coldeflist;
} CompositeTypeStmt;

typedef struct ConstraintsSetStmt
{
NodeTag		type;
	List	   *constraints;	
	bool		deferred;
} ConstraintsSetStmt;

typedef struct CopyStmt
{
NodeTag		type;
	RangeVar   *relation;		
	Node	   *query;			
	List	   *attlist;		
	bool		is_from;		
	bool		is_program;		
	char	   *filename;		
	List	   *options;		
	Node	   *whereClause;
} CopyStmt;

typedef struct CreateAmStmt
{
NodeTag		type;
	char	   *amname;			
	List	   *handler_name;	
	char		amtype;
} CreateAmStmt;

typedef struct CreateCastStmt
{
NodeTag		type;
	TypeName   *sourcetype;
	TypeName   *targettype;
	ObjectWithArgs *func;
	int context;
	bool		inout;
} CreateCastStmt;

typedef struct CreateConversionStmt
{
NodeTag		type;
	List	   *conversion_name;	
	char	   *for_encoding_name;	
	char	   *to_encoding_name;	
	List	   *func_name;		
	bool		def;
} CreateConversionStmt;

typedef struct CreateDomainStmt
{
NodeTag		type;
	List	   *domainname;		
	TypeName   *typeName;		
	CollateClause *collClause;	
	List	   *constraints;
} CreateDomainStmt;

typedef struct CreateEnumStmt
{
NodeTag		type;
	List	   *typeName;		
	List	   *vals;
} CreateEnumStmt;

typedef struct CreateEventTrigStmt
{
NodeTag		type;
	char	   *trigname;		
	char	   *eventname;		
	List	   *whenclause;		
	List	   *funcname;
} CreateEventTrigStmt;

typedef struct CreateExtensionStmt
{
NodeTag		type;
	char	   *extname;
	bool		if_not_exists;	
	List	   *options;
} CreateExtensionStmt;

typedef struct CreateFdwStmt
{
NodeTag		type;
	char	   *fdwname;		
	List	   *func_options;	
	List	   *options;
} CreateFdwStmt;

typedef struct CreateForeignServerStmt
{
NodeTag		type;
	char	   *servername;		
	char	   *servertype;		
	char	   *version;		
	char	   *fdwname;		
	bool		if_not_exists;	
	List	   *options;
} CreateForeignServerStmt;

typedef struct CreateForeignTableStmt
{
CreateStmt	base;
	char	   *servername;
	List	   *options;
} CreateForeignTableStmt;

typedef struct CreateFunctionStmt
{
NodeTag		type;
	bool		is_procedure;	
	bool		replace;		
	List	   *funcname;		
	List	   *parameters;		
	TypeName   *returnType;		
	List	   *options;		
	Node	   *sql_body;
} CreateFunctionStmt;

typedef struct CreateOpClassItem
{
NodeTag		type;
	int			itemtype;		
	ObjectWithArgs *name;		
	int			number;			
	List	   *order_family;	
	List	   *class_args;		
	TypeName   *storedtype;
} CreateOpClassItem;

typedef struct CreateOpClassStmt
{
NodeTag		type;
	List	   *opclassname;	
	List	   *opfamilyname;	
	char	   *amname;			
	TypeName   *datatype;		
	List	   *items;			
	bool		isDefault;
} CreateOpClassStmt;

typedef struct CreateOpFamilyStmt
{
NodeTag		type;
	List	   *opfamilyname;	
	char	   *amname;
} CreateOpFamilyStmt;

typedef struct CreatePLangStmt
{
NodeTag		type;
	bool		replace;		
	char	   *plname;			
	List	   *plhandler;		
	List	   *plinline;		
	List	   *plvalidator;	
	bool		pltrusted;
} CreatePLangStmt;

typedef struct CreatePolicyStmt
{
NodeTag		type;
	char	   *policy_name;	
	RangeVar   *table;			
	char	   *cmd_name;		
	bool		permissive;		
	List	   *roles;			
	Node	   *qual;			
	Node	   *with_check;
} CreatePolicyStmt;

typedef struct CreatePublicationStmt
{
NodeTag		type;
	char	   *pubname;		
	List	   *options;		
	List	   *pubobjects;		
	bool		for_all_tables; 
	bool		for_all_sequences;
} CreatePublicationStmt;

typedef struct CreateRangeStmt
{
NodeTag		type;
	List	   *typeName;		
	List	   *params;
} CreateRangeStmt;

typedef struct CreateRoleStmt
{
NodeTag		type;
	int stmt_type;		
	char	   *role;			
	List	   *options;
} CreateRoleStmt;

typedef struct CreateSchemaStmt
{
NodeTag		type;
	char	   *schemaname;		
	RoleSpec   *authrole;		
	List	   *schemaElts;		
	bool		if_not_exists;
} CreateSchemaStmt;

typedef struct CreateSeqStmt
{
NodeTag		type;
	RangeVar   *sequence;		
	List	   *options;
	int			ownerId;		
	bool		for_identity;
	bool		if_not_exists;
} CreateSeqStmt;

typedef struct CreateStatsStmt
{
NodeTag		type;
	List	   *defnames;		
	List	   *stat_types;		
	List	   *exprs;			
	List	   *relations;		
	char	   *stxcomment;		
	bool		transformed;	
	bool		if_not_exists;	
	int			owner;
} CreateStatsStmt;



typedef struct CreateSubscriptionStmt
{
NodeTag		type;
	char	   *subname;		
	char	   *servername;		
	char	   *conninfo;		
	List	   *publication;	
	List	   *options;
} CreateSubscriptionStmt;

typedef struct CreateTableAsStmt
{
NodeTag		type;
	Node	   *query;			
	IntoClause *into;			
	int	objtype;		
	bool		is_select_into; 
	bool		if_not_exists;
} CreateTableAsStmt;

typedef struct CreateTableSpaceStmt
{
NodeTag		type;
	char	   *tablespacename;
	RoleSpec   *owner;
	char	   *location;
	List	   *options;
} CreateTableSpaceStmt;

typedef struct CreateTransformStmt
{
NodeTag		type;
	bool		replace;
	TypeName   *type_name;
	char	   *lang;
	ObjectWithArgs *fromsql;
	ObjectWithArgs *tosql;
} CreateTransformStmt;

typedef struct CreateTrigStmt
{
NodeTag		type;
	bool		replace;		
	bool		isconstraint;	
	char	   *trigname;		
	RangeVar   *relation;		
	List	   *funcname;		
	List	   *args;			
	bool		row;			
	int16		timing;			
	int16		events;			
	List	   *columns;		
	Node	   *whenClause;		
	List	   *transitionRels; 
	bool		deferrable;		
	bool		initdeferred;	
	RangeVar   *constrrel;
} CreateTrigStmt;

typedef struct CreateUserMappingStmt
{
NodeTag		type;
	RoleSpec   *user;			
	char	   *servername;		
	bool		if_not_exists;	
	List	   *options;
} CreateUserMappingStmt;

typedef struct CreatedbStmt
{
NodeTag		type;
	char	   *dbname;			
	List	   *options;
} CreatedbStmt;

typedef struct CurrentOfExpr
{
Expr		xpr;
	int		cvarno;			
	char	   *cursor_name;	
	int			cursor_param;
} CurrentOfExpr;

typedef struct DeallocateStmt
{
NodeTag		type;
	char	   *name ;
	bool		isall;
	ParseLoc	location ;
} DeallocateStmt;

typedef struct DeclareCursorStmt
{
NodeTag		type;
	char	   *portalname;		
	int			options;		
	Node	   *query;
} DeclareCursorStmt;

typedef struct DefineStmt
{
NodeTag		type;
	int	kind;			
	bool		oldstyle;		
	List	   *defnames;		
	List	   *args;			
	List	   *definition;		
	bool		if_not_exists;	
	bool		replace;
} DefineStmt;

typedef struct DeleteStmt
{
NodeTag		type;
	RangeVar   *relation;		
	List	   *usingClause;	
	Node	   *whereClause;	
	ReturningClause *returningClause;	
	WithClause *withClause;
} DeleteStmt;

typedef struct DiscardStmt
{
NodeTag		type;
	int target;
} DiscardStmt;

typedef struct DoStmt
{
NodeTag		type;
	List	   *args;
} DoStmt;

typedef struct DropOwnedStmt
{
NodeTag		type;
	List	   *roles;
	int behavior;
} DropOwnedStmt;

typedef struct DropRoleStmt
{
NodeTag		type;
	List	   *roles;			
	bool		missing_ok;
} DropRoleStmt;

typedef struct DropStmt
{
NodeTag		type;
	List	   *objects;		
	int	removeType;		
	int behavior;		
	bool		missing_ok;		
	bool		concurrent;
} DropStmt;

typedef struct DropSubscriptionStmt
{
NodeTag		type;
	char	   *subname;		
	bool		missing_ok;		
	int behavior;
} DropSubscriptionStmt;

typedef struct DropTableSpaceStmt
{
NodeTag		type;
	char	   *tablespacename;
	bool		missing_ok;
} DropTableSpaceStmt;

typedef struct DropUserMappingStmt
{
NodeTag		type;
	RoleSpec   *user;			
	char	   *servername;		
	bool		missing_ok;
} DropUserMappingStmt;

typedef struct DropdbStmt
{
NodeTag		type;
	char	   *dbname;			
	bool		missing_ok;		
	List	   *options;
} DropdbStmt;

typedef struct ExecuteStmt
{
NodeTag		type;
	char	   *name;			
	List	   *params;
} ExecuteStmt;

typedef struct ExplainStmt
{
NodeTag		type;
	Node	   *query;			
	List	   *options;
} ExplainStmt;

typedef struct FetchStmt
{
NodeTag		type;
	int direction;	
	long		howMany ;
	char	   *portalname;
	bool		ismove;
	int direction_keyword;
	ParseLoc	location ;
} FetchStmt;



typedef struct FuncCall
{
NodeTag		type;
	List	   *funcname;		
	List	   *args;			
	List	   *agg_order;		
	Node	   *agg_filter;		
	struct WindowDef *over;		
	int			ignore_nulls;	
	bool		agg_within_group;	
	bool		agg_star;		
	bool		agg_distinct;	
	bool		func_variadic;	
	int funcformat;	
	ParseLoc	location;
} FuncCall;

typedef struct FunctionParameter
{
NodeTag		type;
	char	   *name;			
	TypeName   *argType;		
	int mode; 
	Node	   *defexpr;		
	ParseLoc	location;
} FunctionParameter;

typedef struct GrantRoleStmt
{
NodeTag		type;
	List	   *granted_roles;	
	List	   *grantee_roles;	
	bool		is_grant;		
	List	   *opt;			
	RoleSpec   *grantor;		
	int behavior;
} GrantRoleStmt;

typedef struct GrantStmt
{
NodeTag		type;
	bool		is_grant;		
	int targtype;	
	int	objtype;		
	List	   *objects;		
	List	   *privileges;		
	List	   *grantees;		
	bool		grant_option;	
	RoleSpec   *grantor;		
	int behavior;
} GrantStmt;

typedef struct GroupingFunc
{
Expr		xpr;
	List	   *args ;
	List	   *refs ;
	List	   *cols ;
	int		agglevelsup;
	ParseLoc	location;
} GroupingFunc;

typedef struct ImportForeignSchemaStmt
{
NodeTag		type;
	char	   *server_name;	
	char	   *remote_schema;	
	char	   *local_schema;	
	int list_type;	
	List	   *table_list;		
	List	   *options;
} ImportForeignSchemaStmt;

typedef struct IndexElem
{
NodeTag		type;
	char	   *name;			
	Node	   *expr;			
	char	   *indexcolname;	
	List	   *collation;		
	List	   *opclass;		
	List	   *opclassopts;	
	int	ordering;		
	int nulls_ordering; 
	ParseLoc	location;
} IndexElem;

typedef struct IndexStmt
{
NodeTag		type;
	char	   *idxname;		
	RangeVar   *relation;		
	char	   *accessMethod;	
	char	   *tableSpace;		
	List	   *indexParams;	
	List	   *indexIncludingParams;	
	List	   *options;		
	Node	   *whereClause;	
	List	   *excludeOpNames; 
	char	   *idxcomment;		
	int			indexOid;		
	int oldNumber;	
	int oldCreateSubid;	
	int oldFirstRelfilelocatorSubid;	
	bool		unique;			
	bool		nulls_not_distinct; 
	bool		primary;		
	bool		isconstraint;	
	bool		iswithoutoverlaps;	
	bool		deferrable;		
	bool		initdeferred;	
	bool		transformed;	
	bool		concurrent;		
	bool		if_not_exists;	
	bool		reset_default_tblspc;
} IndexStmt;

typedef struct InferClause
{
NodeTag		type;
	List	   *indexElems;		
	Node	   *whereClause;	
	char	   *conname;		
	ParseLoc	location;
} InferClause;

typedef struct InsertStmt
{
NodeTag		type;
	RangeVar   *relation;		
	List	   *cols;			
	Node	   *selectStmt;		
	OnConflictClause *onConflictClause; 
	ReturningClause *returningClause;	
	WithClause *withClause;		
	int override;
} InsertStmt;



typedef struct IntoClause
{
NodeTag		type;
	RangeVar   *rel;			
	List	   *colNames;		
	char	   *accessMethod;	
	List	   *options;		
	int onCommit;	
	char	   *tableSpaceName; 
	void *viewQuery ;
	bool		skipData;
} IntoClause;

typedef struct JoinExpr
{
NodeTag		type;
	int	jointype;		
	bool		isNatural;		
	Node	   *larg;			
	Node	   *rarg;			
	List	   *usingClause ;
	Alias	   *join_using_alias ;
	Node	   *quals;
	Alias	   *alias ;
	int			rtindex;
} JoinExpr;

typedef struct JsonAggConstructor
{
NodeTag		type;
	JsonOutput *output;			
	Node	   *agg_filter;		
	List	   *agg_order;		
	struct WindowDef *over;		
	ParseLoc	location;
} JsonAggConstructor;

typedef struct JsonArgument
{
NodeTag		type;
	JsonValueExpr *val;			
	char	   *name;
} JsonArgument;

typedef struct JsonArrayAgg
{
NodeTag		type;
	JsonAggConstructor *constructor;	
	JsonValueExpr *arg;			
	bool		absent_on_null;
} JsonArrayAgg;

typedef struct JsonArrayConstructor
{
NodeTag		type;
	List	   *exprs;			
	JsonOutput *output;			
	bool		absent_on_null; 
	ParseLoc	location;
} JsonArrayConstructor;

typedef struct JsonArrayQueryConstructor
{
NodeTag		type;
	Node	   *query;			
	JsonOutput *output;			
	JsonFormat *format;			
	bool		absent_on_null; 
	ParseLoc	location;
} JsonArrayQueryConstructor;



typedef struct JsonFuncExpr
{
NodeTag		type;
	int	op;				
	char	   *column_name;	
	JsonValueExpr *context_item;	
	Node	   *pathspec;		
	List	   *passing;		
	JsonOutput *output;			
	JsonBehavior *on_empty;		
	JsonBehavior *on_error;		
	int wrapper;		
	int	quotes;			
	ParseLoc	location;
} JsonFuncExpr;

typedef struct JsonObjectAgg
{
NodeTag		type;
	JsonAggConstructor *constructor;	
	JsonKeyValue *arg;			
	bool		absent_on_null; 
	bool		unique;
} JsonObjectAgg;

typedef struct JsonObjectConstructor
{
NodeTag		type;
	List	   *exprs;			
	JsonOutput *output;			
	bool		absent_on_null; 
	bool		unique;			
	ParseLoc	location;
} JsonObjectConstructor;

typedef struct JsonOutput
{
NodeTag		type;
	TypeName   *typeName;		
	JsonReturning *returning;
} JsonOutput;

typedef struct JsonParseExpr
{
NodeTag		type;
	JsonValueExpr *expr;		
	JsonOutput *output;			
	bool		unique_keys;	
	ParseLoc	location;
} JsonParseExpr;

typedef struct JsonReturning
{
NodeTag		type;
	JsonFormat *format;			
	int			typid;			
	int32		typmod;
} JsonReturning;

typedef struct JsonScalarExpr
{
NodeTag		type;
	Expr	   *expr;			
	JsonOutput *output;			
	ParseLoc	location;
} JsonScalarExpr;

typedef struct JsonSerializeExpr
{
NodeTag		type;
	JsonValueExpr *expr;		
	JsonOutput *output;			
	ParseLoc	location;
} JsonSerializeExpr;

typedef struct JsonTable
{
NodeTag		type;
	JsonValueExpr *context_item;	
	JsonTablePathSpec *pathspec;	
	List	   *passing;		
	List	   *columns;		
	JsonTablePlanSpec *planspec;	
	JsonBehavior *on_error;		
	Alias	   *alias;			
	bool		lateral;		
	ParseLoc	location;
} JsonTable;

typedef struct JsonTableColumn
{
NodeTag		type;
	int coltype;	
	char	   *name;			
	TypeName   *typeName;		
	JsonTablePathSpec *pathspec;	
	JsonFormat *format;			
	int wrapper;		
	int	quotes;			
	List	   *columns;		
	JsonBehavior *on_empty;		
	JsonBehavior *on_error;		
	ParseLoc	location;
} JsonTableColumn;

typedef struct JsonTablePlanSpec
{
NodeTag		type;
	int plan_type;	
	int join_type;	
	char	   *pathname;		
	struct JsonTablePlanSpec *plan1;	
	struct JsonTablePlanSpec *plan2;	
	ParseLoc	location;
} JsonTablePlanSpec;

typedef struct ListenStmt
{
NodeTag		type;
	char	   *conditionname;
} ListenStmt;

typedef struct LoadStmt
{
NodeTag		type;
	char	   *filename;
} LoadStmt;

typedef struct LockStmt
{
NodeTag		type;
	List	   *relations;		
	int			mode;			
	bool		nowait;
} LockStmt;

typedef struct LockingClause
{
NodeTag		type;
	List	   *lockedRels;		
	int strength;
	int waitPolicy;
} LockingClause;

typedef struct MergeStmt
{
NodeTag		type;
	RangeVar   *relation;		
	Node	   *sourceRelation; 
	Node	   *joinCondition;	
	List	   *mergeWhenClauses;	
	ReturningClause *returningClause;	
	WithClause *withClause;
} MergeStmt;

typedef struct MergeSupportFunc
{
Expr		xpr;
	int			msftype;
	int			msfcollid;
	ParseLoc	location;
} MergeSupportFunc;

typedef struct MergeWhenClause
{
NodeTag		type;
	int matchKind;	
	int		commandType;	
	int override;	
	Node	   *condition;		
	List	   *targetList;		
	List	   *values;
} MergeWhenClause;

typedef struct MinMaxExpr
{
Expr		xpr;
	int			minmaxtype ;
	int			minmaxcollid ;
	int			inputcollid ;
	int	op;
	List	   *args;
	ParseLoc	location;
} MinMaxExpr;

typedef struct MultiAssignRef
{
NodeTag		type;
	Node	   *source;			
	int			colno;			
	int			ncolumns;
} MultiAssignRef;

typedef struct NamedArgExpr
{
Expr		xpr;
	Expr	   *arg;
	char	   *name ;
	int			argnumber;
	ParseLoc	location;
} NamedArgExpr;

typedef struct NotifyStmt
{
NodeTag		type;
	char	   *conditionname;	
	char	   *payload;
} NotifyStmt;

typedef struct NullTest
{
Expr		xpr;
	Expr	   *arg;			
	int nulltesttype;	
	bool		argisrow ;
	ParseLoc	location;
} NullTest;

typedef struct ObjectWithArgs
{
NodeTag		type;
	List	   *objname;		
	List	   *objargs;		
	List	   *objfuncargs;	
	bool		args_unspecified;
} ObjectWithArgs;

typedef struct OnConflictClause
{
NodeTag		type;
	int action;	
	InferClause *infer;			
	int lockStrength;	
	List	   *targetList;		
	Node	   *whereClause;	
	ParseLoc	location;
} OnConflictClause;

typedef struct PLAssignStmt
{
NodeTag		type;
	char	   *name;			
	List	   *indirection;	
	int			nnames;			
	SelectStmt *val;			
	ParseLoc	location;
} PLAssignStmt;

typedef struct ParamRef
{
NodeTag		type;
	int			number;			
	ParseLoc	location;
} ParamRef;

typedef struct PartitionCmd
{
NodeTag		type;
	RangeVar   *name;			
	PartitionBoundSpec *bound;	
	bool		concurrent;
} PartitionCmd;

typedef struct PartitionElem
{
NodeTag		type;
	char	   *name;			
	Node	   *expr;			
	List	   *collation;		
	List	   *opclass;		
	ParseLoc	location;
} PartitionElem;

typedef struct PartitionSpec
{
NodeTag		type;
	int strategy;
	List	   *partParams;		
	ParseLoc	location;
} PartitionSpec;

typedef struct PrepareStmt
{
NodeTag		type;
	char	   *name;			
	List	   *argtypes;		
	Node	   *query;
} PrepareStmt;

typedef struct PublicationAllObjSpec
{
NodeTag		type;
	int pubobjtype;	
	List	   *except_tables;	
	ParseLoc	location;
} PublicationAllObjSpec;

typedef struct PublicationObjSpec
{
NodeTag		type;
	int pubobjtype;	
	char	   *name;
	PublicationTable *pubtable;
	ParseLoc	location;
} PublicationObjSpec;

typedef struct PublicationTable
{
NodeTag		type;
	RangeVar   *relation;		
	Node	   *whereClause;	
	List	   *columns;		
	bool		except;
} PublicationTable;

typedef struct RangeFunction
{
NodeTag		type;
	bool		lateral;		
	bool		ordinality;		
	bool		is_rowsfrom;	
	List	   *functions;		
	Alias	   *alias;			
	List	   *coldeflist;
} RangeFunction;

typedef struct RangeSubselect
{
NodeTag		type;
	bool		lateral;		
	Node	   *subquery;		
	Alias	   *alias;
} RangeSubselect;

typedef struct RangeTableFunc
{
NodeTag		type;
	bool		lateral;		
	Node	   *docexpr;		
	Node	   *rowexpr;		
	List	   *namespaces;		
	List	   *columns;		
	Alias	   *alias;			
	ParseLoc	location;
} RangeTableFunc;

typedef struct RangeTableFuncCol
{
NodeTag		type;
	char	   *colname;		
	TypeName   *typeName;		
	bool		for_ordinality; 
	bool		is_not_null;	
	Node	   *colexpr;		
	Node	   *coldefexpr;		
	ParseLoc	location;
} RangeTableFuncCol;

typedef struct RangeTableSample
{
NodeTag		type;
	Node	   *relation;		
	List	   *method;			
	List	   *args;			
	Node	   *repeatable;		
	ParseLoc	location;
} RangeTableSample;

typedef struct RangeVar
{
NodeTag		type;
	char	   *catalogname;
	char	   *schemaname;
	char	   *relname;
	bool		inh;
	char		relpersistence;
	Alias	   *alias;
	ParseLoc	location;
} RangeVar;

typedef struct RawStmt
{
NodeTag		type;
	Node	   *stmt;			
	ParseLoc	stmt_location;	
	ParseLoc	stmt_len;
} RawStmt;

typedef struct ReassignOwnedStmt
{
NodeTag		type;
	List	   *roles;
	RoleSpec   *newrole;
} ReassignOwnedStmt;

typedef struct RefreshMatViewStmt
{
NodeTag		type;
	bool		concurrent;		
	bool		skipData;		
	RangeVar   *relation;
} RefreshMatViewStmt;

typedef struct ReindexStmt
{
NodeTag		type;
	int kind;		
	RangeVar   *relation;		
	const char *name;			
	List	   *params;
} ReindexStmt;

typedef struct RenameStmt
{
NodeTag		type;
	int	renameType;		
	int	relationType;	
	RangeVar   *relation;		
	Node	   *object;			
	char	   *subname;		
	char	   *newname;		
	int behavior;		
	bool		missing_ok;
} RenameStmt;

typedef struct RepackStmt
{
NodeTag		type;
	int command;		
	VacuumRelation *relation;	
	char	   *indexname;		
	bool		usingindex;		
	List	   *params;
} RepackStmt;

typedef struct ReplicaIdentityStmt
{
NodeTag		type;
	char		identity_type;
	char	   *name;
} ReplicaIdentityStmt;

typedef struct ResTarget
{
NodeTag		type;
	char	   *name;			
	List	   *indirection;	
	Node	   *val;			
	ParseLoc	location;
} ResTarget;

typedef struct ReturnStmt
{
NodeTag		type;
	Node	   *returnval;
} ReturnStmt;

typedef struct ReturningClause
{
NodeTag		type;
	List	   *options;		
	List	   *exprs;
} ReturningClause;

typedef struct ReturningOption
{
NodeTag		type;
	int option; 
	char	   *value;			
	ParseLoc	location;
} ReturningOption;

typedef struct RoleSpec
{
NodeTag		type;
	int roletype;		
	char	   *rolename;		
	ParseLoc	location;
} RoleSpec;

typedef struct RowExpr
{
Expr		xpr;
	List	   *args;			
	int			row_typeid ;
	int row_format ;
	List	   *colnames ;
	ParseLoc	location;
} RowExpr;

typedef struct RuleStmt
{
NodeTag		type;
	RangeVar   *relation;		
	char	   *rulename;		
	Node	   *whereClause;	
	int		event;			
	bool		instead;		
	List	   *actions;		
	bool		replace;
} RuleStmt;

typedef struct SQLValueFunction
{
Expr		xpr;
	int op;		
	int			type ;
	int32		typmod;
	ParseLoc	location;
} SQLValueFunction;

typedef struct SecLabelStmt
{
NodeTag		type;
	int	objtype;		
	Node	   *object;			
	char	   *provider;		
	char	   *label;
} SecLabelStmt;

typedef struct SelectStmt
{
NodeTag		type;
	List	   *distinctClause; 
	IntoClause *intoClause;		
	List	   *targetList;		
	List	   *fromClause;		
	Node	   *whereClause;	
	List	   *groupClause;	
	bool		groupDistinct;	
	Node	   *havingClause;	
	List	   *windowClause;	
	List	   *valuesLists;	
	List	   *sortClause;		
	Node	   *limitOffset;	
	Node	   *limitCount;		
	int limitOption;	
	List	   *lockingClause;	
	WithClause *withClause;		
	int op;			
	bool		all;			
	struct SelectStmt *larg;	
	struct SelectStmt *rarg;
} SelectStmt;

typedef struct SetToDefault
{
Expr		xpr;
	int			typeId;
	int32		typeMod ;
	int			collation ;
	ParseLoc	location;
} SetToDefault;

typedef struct SortBy
{
NodeTag		type;
	Node	   *node;			
	int	sortby_dir;		
	int sortby_nulls;	
	List	   *useOp;			
	ParseLoc	location;
} SortBy;

typedef struct StatsElem
{
NodeTag		type;
	char	   *name;			
	Node	   *expr;
} StatsElem;



typedef struct SubLink
{
Expr		xpr;
	int subLinkType;	
	int			subLinkId;		
	Node	   *testexpr;		
	List	   *operName ;
	Node	   *subselect;
	ParseLoc	location;
} SubLink;

typedef struct TableLikeClause
{
NodeTag		type;
	RangeVar   *relation;
	uint32		options;		
	int			relationOid;
} TableLikeClause;

typedef struct TransactionStmt
{
NodeTag		type;
	int kind;	
	List	   *options;		
	char	   *savepoint_name ;
	char	   *gid ;
	bool		chain;			
	ParseLoc	location ;
} TransactionStmt;

typedef struct TriggerTransition
{
NodeTag		type;
	char	   *name;
	bool		isNew;
	bool		isTable;
} TriggerTransition;

typedef struct TruncateStmt
{
NodeTag		type;
	List	   *relations;		
	bool		restart_seqs;	
	int behavior;
} TruncateStmt;

typedef struct TypeCast
{
NodeTag		type;
	Node	   *arg;			
	TypeName   *typeName;		
	ParseLoc	location;
} TypeCast;

typedef struct UnlistenStmt
{
NodeTag		type;
	char	   *conditionname;
} UnlistenStmt;

typedef struct UpdateStmt
{
NodeTag		type;
	RangeVar   *relation;		
	List	   *targetList;		
	Node	   *whereClause;	
	List	   *fromClause;		
	ReturningClause *returningClause;	
	WithClause *withClause;
} UpdateStmt;

typedef struct VacuumRelation
{
NodeTag		type;
	RangeVar   *relation;		
	int			oid;			
	List	   *va_cols;
} VacuumRelation;

typedef struct VacuumStmt
{
NodeTag		type;
	List	   *options;		
	List	   *rels;			
	bool		is_vacuumcmd;
} VacuumStmt;

typedef struct VariableSetStmt
{
NodeTag		type;
	int kind;
	char	   *name;
	List	   *args;
	bool		jumble_args;
	bool		is_local;
	ParseLoc	location ;
} VariableSetStmt;

typedef struct VariableShowStmt
{
NodeTag		type;
	char	   *name;
} VariableShowStmt;

typedef struct ViewStmt
{
NodeTag		type;
	RangeVar   *view;			
	List	   *aliases;		
	Node	   *query;			
	bool		replace;		
	List	   *options;		
	int withCheckOption;
} ViewStmt;

typedef struct WaitStmt
{
NodeTag		type;
	char	   *lsn_literal ;
	List	   *options;
	ParseLoc	lsn_location ;
} WaitStmt;

typedef struct WindowDef
{
NodeTag		type;
	char	   *name;			
	char	   *refname;		
	List	   *partitionClause;	
	List	   *orderClause;	
	int			frameOptions;	
	Node	   *startOffset;	
	Node	   *endOffset;		
	ParseLoc	location;
} WindowDef;

typedef struct WithClause
{
NodeTag		type;
	List	   *ctes;			
	bool		recursive;		
	ParseLoc	location;
} WithClause;

typedef struct XmlExpr
{
Expr		xpr;
	int	op;
	char	   *name ;
	List	   *named_args;
	List	   *arg_names ;
	List	   *args;
	int xmloption ;
	bool		indent;
	int			type ;
	int32		typmod ;
	ParseLoc	location;
} XmlExpr;

typedef struct XmlSerialize
{
NodeTag		type;
	int xmloption;	
	Node	   *expr;
	TypeName   *typeName;
	bool		indent;			
	ParseLoc	location;
} XmlSerialize;

typedef struct ErrorSaveContext
{
	NodeTag		type;
	bool		error_occurred;
	bool		details_wanted;
	void	   *error_data;
} ErrorSaveContext;

typedef struct Constraint
{
	NodeTag		type;
	int		contype;
	char	   *conname;
	bool		deferrable;
	bool		initdeferred;
	ParseLoc	location;
	bool		is_no_inherit;
	Node	   *raw_expr;
	char	   *cooked_expr;
	char		generated_when;
	List	   *keys;
	List	   *including;
	List	   *exclusions;
	List	   *options;
	char	   *indexname;
	char	   *access_method;
	Node	   *where_clause;
	RangeVar   *pktable;
	List	   *fk_attrs;
	List	   *pk_attrs;
	char		fk_matchtype;
	char		fk_upd_action;
	char		fk_del_action;
	int		old_fk_prod_idx;
	bool		skip_validation;
	bool		initially_valid;
	bool		is_enforced;
	bool		nulls_not_distinct;
	char	   *indexspace;
	char		generated_kind;
	List	   *fk_del_set_cols;
	bool		without_overlaps;
	bool		fk_with_period;
	bool		pk_with_period;
} Constraint;
