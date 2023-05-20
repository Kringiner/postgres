
SELECT setseed(1);

-- Test that index check works
CREATE TABLE hash_test AS SELECT (random()* 10000 + 1) num from generate_series(1,10000);
CREATE INDEX hash_idx ON hash_test USING hash(num);
SELECT hash_index_check('hash_idx');