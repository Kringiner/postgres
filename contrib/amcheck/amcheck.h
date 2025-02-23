/*-------------------------------------------------------------------------
 *
 * amcheck.h
 *		Shared routines for amcheck verifications.
 *
 * Copyright (c) 2017-2023, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  contrib/amcheck/amcheck.h
 *
 *-------------------------------------------------------------------------
 */
#include "storage/bufpage.h"
#include "storage/lmgr.h"
#include "storage/lockdefs.h"
#include "utils/relcache.h"
#include "miscadmin.h"

/* Typedefs for callback functions for amcheck_lock_relation */
typedef void (*IndexCheckableCallback) (Relation index);
typedef void (*IndexDoCheckCallback) (Relation rel,
									  Relation heaprel,
									  void *state);

extern void amcheck_lock_relation_and_check(Oid indrelid,
											Oid am_id,
											IndexDoCheckCallback check,
											LOCKMODE lockmode, void *state);

extern void index_checkable(Relation rel, Oid am_id);
