#include <odyssey.h>
#include <pg_compat.h>
#include <stdarg.h>
#include <sql/full/makefuncs.h>
#include <sql/full/ast.h>
#include <sql/full/scanner.h>

String *
makeString(char *str)
{
	String *s = makeNode(String);
	s->sval = str;
	return s;
}

Integer *
makeInteger(int ival)
{
	Integer *i = makeNode(Integer);
	i->ival = ival;
	return i;
}

Float *
makeFloat(char *str)
{
	Float *f = makeNode(Float);
	f->fval = str;
	return f;
}

Boolean *
makeBoolean(bool state)
{
	Boolean *b = makeNode(Boolean);
	b->boolval = state;
	return b;
}

Null *
makeNull(void)
{
	return makeNode(Null);
}

A_Star *
makeAStar(void)
{
	return makeNode(A_Star);
}

RangeVar *
makeRangeVar(char *schemaname, char *relname, ParseLoc location)
{
	RangeVar *r = makeNode(RangeVar);
	r->catalogname = NULL;
	r->schemaname = schemaname;
	r->relname = relname;
	r->inh = true;
	r->relpersistence = 'p';
	r->alias = NULL;
	r->location = location;
	return r;
}



TypeName *
makeTypeName(char *typnam)
{
	TypeName *n = makeNode(TypeName);
	n->names = list_make1(makeString(typnam));
	n->typeOid = 0;
	n->setof = false;
	n->arrayBounds = NIL;
	n->location = -1;
	return n;
}

TypeName *
makeTypeNameFromNameList(List *names)
{
	TypeName *n = makeNode(TypeName);
	n->names = names;
	n->typeOid = 0;
	n->setof = false;
	n->arrayBounds = NIL;
	n->location = -1;
	return n;
}

DefElem *
makeDefElem(char *name, Node *arg, ParseLoc location)
{
	DefElem *n = makeNode(DefElem);
	n->defname = name;
	n->arg = arg;
	n->defaction = 0;
	n->location = location;
	return n;
}





Node *
makeStringConst(char *str, ParseLoc location)
{
	A_Const *n = makeNode(A_Const);
	n->val.sval.type = T_String; n->val.sval.sval = str;
	n->location = location;
	return (Node *) n;
}

DefElem *
makeDefElemExtended(char *nameSpace, char *name, Node *arg, int defaction, ParseLoc location)
{
	DefElem *n = makeNode(DefElem);
	n->defnamespace = nameSpace;
	n->defname = name;
	n->arg = arg;
	n->defaction = defaction;
	n->location = location;
	return n;
}

VacuumRelation *
makeVacuumRelation(RangeVar *relation, int oid, List *va_cols)
{
	VacuumRelation *n = makeNode(VacuumRelation);
	n->relation = relation;
	n->oid = oid;
	n->va_cols = va_cols;
	return n;
}

A_Expr *
makeSimpleA_Expr(int kind, char *name, Node *lexpr, Node *rexpr, ParseLoc location)
{
	A_Expr *n = makeNode(A_Expr);
	n->kind = kind;
	n->name = list_make1(makeString(name));
	n->lexpr = lexpr;
	n->rexpr = rexpr;
	n->location = location;
	return n;
}

GroupingSet *
makeGroupingSet(int kind, List *content, ParseLoc location)
{
	GroupingSet *n = makeNode(GroupingSet);
	n->kind = kind;
	n->content = content;
	n->location = location;
	return n;
}

JsonTablePathSpec *makeJsonTablePathSpec(char *string, char *name, ParseLoc string_loc, ParseLoc name_loc)
{ (void)string_loc; (void)name_loc; JsonTablePathSpec *n = makeNode(JsonTablePathSpec); n->string = string; n->name = name; return n; }

JsonFormat *makeJsonFormat(int type, int encoding, ParseLoc location)
{ (void)location; JsonFormat *n = makeNode(JsonFormat); n->format_type = type; n->encoding = encoding; return n; }

JsonValueExpr *makeJsonValueExpr(Node *raw_expr, Node *formatted_expr, JsonFormat *format)
{ JsonValueExpr *n = makeNode(JsonValueExpr); n->raw_expr = raw_expr; n->formatted_expr = formatted_expr; n->format = format; return n; }

Node *makeJsonKeyValue(Node *key, Node *value)
{ (void)key; (void)value; return NULL; }

Node *makeJsonIsPredicate(Node *expr, JsonFormat *format, int item_type, bool unique_keys, int exprBaseType, ParseLoc location)
{ (void)expr; (void)format; (void)item_type; (void)unique_keys; (void)exprBaseType; (void)location; return NULL; }

JsonBehavior *makeJsonBehavior(int btype, Node *expr, ParseLoc location)
{ (void)location; JsonBehavior *n = makeNode(JsonBehavior); n->btype = btype; n->expr = expr; return n; }

JsonTablePath *makeJsonTablePath(Node *pathvalue, char *pathname)
{ (void)pathvalue; (void)pathname; return NULL; }

Node *makeJsonTableDefaultPlan(int join_type, ParseLoc location)
{ (void)join_type; (void)location; return NULL; }

Node *makeJsonTableSimplePlan(char *pathname, ParseLoc location)
{ (void)pathname; (void)location; return NULL; }

Node *makeJsonTableJoinedPlan(int type, Node *plan1, Node *plan2, ParseLoc location)
{ (void)type; (void)plan1; (void)plan2; (void)location; return NULL; }

Node *makeAArrayExpr(List *elements, ParseLoc location, ParseLoc location_end)
{ (void)location_end; A_ArrayExpr *n = makeNode(A_ArrayExpr); n->elements = elements; n->location = location; return (Node *) n; }

Node *makeSQLValueFunction(int op, int32 typmod, ParseLoc location)
{ (void)op; (void)typmod; (void)location; return NULL; }

Node *makeXmlExpr(int op, char *name, List *named_args, List *args, ParseLoc location)
{ (void)op; (void)name; (void)named_args; (void)args; (void)location; return NULL; }

Alias *makeAlias(char *aliasname, List *colnames)
{ Alias *a = makeNode(Alias); a->aliasname = aliasname; a->colnames = colnames; return a; }

FuncCall *
makeFuncCall(List *name, List *args, int funcformat, ParseLoc location)
{
	FuncCall *n = makeNode(FuncCall);
	n->funcname = name;
	n->args = args;
	n->funcformat = funcformat;
	n->location = location;
	return n;
}

A_Expr *
makeA_Expr(int kind, List *name, Node *lexpr, Node *rexpr, ParseLoc location)
{
	A_Expr *n = makeNode(A_Expr);
	n->kind = kind;
	n->name = name;
	n->lexpr = lexpr;
	n->rexpr = rexpr;
	n->location = location;
	return n;
}

Node *makeBoolExpr(int boolop, List *args, ParseLoc location)
{
	BoolExpr *n = makeNode(BoolExpr);
	n->boolop = boolop;
	n->args = args;
	n->location = location;
	return (Node *) n;
}

void *
copyObject(const void *from)
{
	(void)from;
	return NULL;
}

char *
psprintf(const char *fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	return pstrdup(buf);
}
