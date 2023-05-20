#include "postgres.h"

#include "amcheck.h"
#include "access/hash.h"


PG_FUNCTION_INFO_V1(hash_index_check);

static void hash_check_keys_consistency(Relation rel, Relation heaprel, void *callback_state);
static void hash_check_bucket_chain(Relation rel, Buffer bucket_buf, BlockNumber bucket_blkno,
                                    BufferAccessStrategy bstrategy);

/*
 * hash_index_check(index regclass)
 *
 * Verify integrity of Hash index.
 *
 * Acquires AccessShareLock on heap & index relations.
 */
Datum
hash_index_check(PG_FUNCTION_ARGS)
{
    Oid indrelid = PG_GETARG_OID(0);

    amcheck_lock_relation_and_check(indrelid,
                                    HASH_AM_OID,
                                    hash_check_keys_consistency,
                                    AccessShareLock,
                                    NULL);

    PG_RETURN_VOID();
}

static void hash_check_bucket_chain(Relation rel, Buffer bucket_buf,
                                    BlockNumber bucket_blkno, BufferAccessStrategy bstrategy)
{
    BlockNumber blkno = bucket_blkno;
    Buffer		buf = bucket_buf;

    /* Scans each page in bucket */
    for(;;)
    {
        HashPageOpaque opaque;
        OffsetNumber offno;
        OffsetNumber maxoffno;
        Page		page;
        Buffer		next_buf;

        /* Get bucket chain */
        page = BufferGetPage(buf);
        opaque = HashPageGetOpaque(page);


        /* Get bucket chain max offset number */
        maxoffno = PageGetMaxOffsetNumber(page);

        /* Scans each tuple in page */
        for(offno = FirstOffsetNumber;
            offno <= maxoffno;
            offno = OffsetNumberNext(offno))
        {
            uint32 hash_key;
            IndexTuple	itup;

            itup = (IndexTuple) PageGetItem(page,
                                            PageGetItemId(page, offno));

            /* Scan hash_key to verify we can access it */
            hash_key = _hash_get_indextuple_hashkey(itup);
        }

        /* Get block number of next page */
        blkno = opaque->hasho_nextblkno;

        /* bail out if there are no more pages to scan. */
        if (!BlockNumberIsValid(blkno))
            break;

        /* Get and lock next buffer */
        next_buf = _hash_getbuf_with_strategy(rel, blkno, HASH_READ,
                                         LH_OVERFLOW_PAGE,
                                         bstrategy);

        /* release current buffer */
        if (buf != bucket_buf)
            _hash_relbuf(rel, buf);

        /* Advance to next page */
        buf = next_buf;
    }
}


static void hash_check_keys_consistency(Relation rel, Relation heaprel, void *callback_state)
{
    BufferAccessStrategy strategy = GetAccessStrategy(BAS_BULKREAD);
    Buffer		metabuf;
    Bucket		orig_maxbucket;
    Bucket		cur_bucket;
    HashMetaPage meta_page;

    /* Get metapage and lock it buffer */
    metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_READ, LH_META_PAGE);
    meta_page = HashPageGetMeta(BufferGetPage(metabuf));

    /* Get iteration boundaries */
    orig_maxbucket = meta_page->hashm_maxbucket;
    cur_bucket = 0;

    /* Scans each bucket */
    while (cur_bucket <= orig_maxbucket)
    {
        BlockNumber bucket_blkno;
        Buffer		buf;

        /* Get address of bucket's start page */
        bucket_blkno = BUCKET_TO_BLKNO(meta_page, cur_bucket);

        /*
         * Get buffer of bucket's start page
         * and lock it
         */
        buf = ReadBufferExtended(rel, MAIN_FORKNUM, bucket_blkno, RBM_NORMAL, strategy);
        LockBuffer(buf, HASH_READ);

        /* Check the format of bucket's start page */
        _hash_checkpage(rel, buf, LH_BUCKET_PAGE);

        hash_check_bucket_chain(rel,buf,bucket_blkno, strategy);

        /* release buffer of bucket start page */
        _hash_relbuf(rel, buf);

        /* Advance to next bucket */
        cur_bucket++;
    }

    /* release meta page buffer after scan */
    _hash_relbuf(rel, metabuf);
}



