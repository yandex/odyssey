#pragma once

#include <sql/full/nodes.h>
#include <sql/full/ast.h>
#include <sql/full/pg_list.h>
#include <sql/full/scanner.h>

String *makeString(char *str);
Integer *makeInteger(int ival);
Float *makeFloat(char *str);
Boolean *makeBoolean(bool state);
Null *makeNull(void);
A_Star *makeAStar(void);
RangeVar *makeRangeVar(char *schemaname, char *relname, ParseLoc location);
Alias *makeAlias(char *aliasname, List *colnames);
TypeName *makeTypeName(char *typnam);
TypeName *makeTypeNameFromNameList(List *names);
DefElem *makeDefElem(char *name, Node *arg, ParseLoc location);

Node *makeStringConst(char *str, ParseLoc location);
DefElem *makeDefElemExtended(char *nameSpace, char *name, Node *arg, int defaction, ParseLoc location);
VacuumRelation *makeVacuumRelation(RangeVar *relation, int oid, List *va_cols);
A_Expr *makeSimpleA_Expr(int kind, char *name, Node *lexpr, Node *rexpr, ParseLoc location);
GroupingSet *makeGroupingSet(int kind, List *content, ParseLoc location);
JsonTablePathSpec *makeJsonTablePathSpec(char *string, char *name, ParseLoc string_loc, ParseLoc name_loc);
JsonFormat *makeJsonFormat(int type, int encoding, ParseLoc location);
JsonValueExpr *makeJsonValueExpr(Node *raw_expr, Node *formatted_expr, JsonFormat *format);
Node *makeJsonKeyValue(Node *key, Node *value);
Node *makeJsonIsPredicate(Node *expr, JsonFormat *format, int item_type, bool unique_keys, int exprBaseType, ParseLoc location);
JsonBehavior *makeJsonBehavior(int btype, Node *expr, ParseLoc location);
JsonTablePath *makeJsonTablePath(Node *pathvalue, char *pathname);
Node *makeJsonTableDefaultPlan(int join_type, ParseLoc location);
Node *makeJsonTableSimplePlan(char *pathname, ParseLoc location);
Node *makeJsonTableJoinedPlan(int type, Node *plan1, Node *plan2, ParseLoc location);
Alias *makeAlias(char *aliasname, List *colnames);
FuncCall *makeFuncCall(List *name, List *args, int funcformat, ParseLoc location);
A_Expr *makeA_Expr(int kind, List *name, Node *lexpr, Node *rexpr, ParseLoc location);
