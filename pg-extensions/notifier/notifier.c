#include "postgres.h"

#include "catalog/pg_class.h"
#include "tcop/utility.h"

PG_MODULE_MAGIC;

static ProcessUtility_hook_type prev_ProcessUtility_hook = NULL;

static void notifyOdysseyAboutTemporaryObject()
{
	elog(NOTICE, "__odyssey_temporary_object_created");

	// pq_putmessage_v2('O', "\x0\x0\x0\x4", 4);
}

static void notifyOdysseyAboutTemporaryTables(Node *parsetree)
{
	CreateStmt *create = (CreateStmt *)parsetree;

	if (create->relation->relpersistence == RELPERSISTENCE_TEMP) {
		notifyOdysseyAboutTemporaryObject();
	}
}

static void
notifier_ProcessUtility_hook(PlannedStmt *pstmt, const char *queryString,
			     bool readOnlyTree, ProcessUtilityContext context,
			     ParamListInfo params, QueryEnvironment *queryEnv,
			     DestReceiver *dest, QueryCompletion *qc)
{
	Node *parsetree = pstmt->utilityStmt;

	switch (nodeTag(parsetree)) {
	case T_CreateStmt:
		notifyOdysseyAboutTemporaryTables(parsetree);
		break;
	default:
		/* nothing to do */
		break;
	}

	return prev_ProcessUtility_hook(pstmt, queryString, readOnlyTree,
					context, params, queryEnv, dest, qc);
}

void _PG_init(void)
{
	prev_ProcessUtility_hook = ProcessUtility_hook ?
					   ProcessUtility_hook :
					   standard_ProcessUtility;

	ProcessUtility_hook = notifier_ProcessUtility_hook;
}