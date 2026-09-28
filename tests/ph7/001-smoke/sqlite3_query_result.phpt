--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SQLite3::query() runs the statement once and hands back a cursor over it
--DESCRIPTION--
`query()` is not a lazy handle: it prepares, STEPS once and rewinds, and only
then answers a SQLite3Result. The step really runs, which is why
`query("INSERT ...")` inserts -- and why fetching from that result inserts a
SECOND time, since the fetch steps the same statement again. Only the FIRST
statement of the string is ever prepared, so `SELECT 1; garbage` answers a row
and never sees the garbage, where exec() runs the whole string and refuses on
the tail.

The cursor itself keeps no state of its own. `numColumns()` and `columnName()`
read the STATEMENT, so they answer before any row has been read; `columnType()`
reads the ROW, so it answers only while one is up -- false before the first
fetch and false again once the walk has run out. Past that gate it has no range
check at all: a column that is not there is SQLITE3_NULL rather than false, and
asking leaves `column index out of range` on the CONNECTION, which the same
question to columnName() never does.

Nothing latches a finished walk and nothing resets it: a statement prepared
with prepare_v2 rewinds itself when it is stepped after SQLITE_DONE, so a fetch
past the end answers false and the NEXT one starts over. That is also why
fetchAll() hands back what is AHEAD of the cursor rather than the whole set,
and why the end of a walk leaves SQLITE_DONE on the handle for lastErrorCode()
to report as 101.

`finalize()` releases this result's hold on the statement rather than closing
it, but the result itself is spent either way -- a second finalize() is the
Error. `close()` on the DATABASE is the other way round: php cleans the list of
everything the connection handed out, so it finalizes the statement under a
result a script is still walking. Dropping the last reference to the connection
does neither, because a live statement retains the object and there is no last
reference to drop.

And `querySingle()` has no cursor to hand back at all, so "no row" takes the
shape the caller asked for: NULL for a scalar, the EMPTY ARRAY for a whole
row.
--FILE--
<?php
$sq4dv = function ($v) use (&$sq4dv) {
    if (is_array($v)) {
        $o = [];
        foreach ($v as $k => $x) { $o[] = var_export($k, true) . '=>' . $sq4dv($x); }
        return '[' . implode(', ', $o) . ']';
    }
    if (is_string($v)) { return ctype_print($v) ? '"' . $v . '"' : 'hex:' . bin2hex($v); }
    if (is_object($v)) { return 'obj:' . get_class($v); }
    return var_export($v, true);
};
$sq4show = function ($label, $fn) use ($sq4dv) {
    $notes = [];
    set_error_handler(function ($no, $str) use (&$notes) { $notes[] = "[$no] $str"; return true; });
    try { $out = $sq4dv($fn()); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    foreach ($notes as $n) { echo '  ', $n, "\n"; }
    echo str_pad($label, 34), ' => ', $out, "\n";
};
$sq4db = new SQLite3(':memory:');
$sq4db->exec('CREATE TABLE q (a INTEGER, b TEXT, c REAL, d BLOB, e)');
$sq4db->exec("INSERT INTO q VALUES (1,'x',1.5,x'00ff',NULL),(2,'y',2.5,x'0102',NULL)");

/* what query() answers, and what it has already DONE by then */
$sq4show('query a select', fn () => $sq4db->query('SELECT a FROM q ORDER BY a'));
$sq4show('query a write RAN it', function () use ($sq4db) {
    $sq4db->query("INSERT INTO q VALUES (3,'z',3.5,NULL,NULL)");
    return $sq4db->querySingle('SELECT count(*) FROM q'); });
$sq4show('and its result RUNS IT AGAIN', function () use ($sq4db) {
    $r = $sq4db->query("INSERT INTO q VALUES (4,'w',4.5,NULL,NULL)");
    $mid = $sq4db->querySingle('SELECT count(*) FROM q');
    $f = $r->fetchArray();
    return [$mid, $f, $sq4db->querySingle('SELECT count(*) FROM q')]; });
$sq4show('only the FIRST statement', fn () => $sq4db->query('SELECT 1; garbage')->fetchArray(SQLITE3_NUM));
$sq4show('query broken', fn () => $sq4db->query('NOT SQL'));
$sq4show('query that fails at the STEP', function () use ($sq4db) {
    return $sq4db->query('SELECT abs(-9223372036854775807-1)'); });
$sq4show('query empty', fn () => $sq4db->query(''));
$sq4show('query a comment', fn () => $sq4db->query('-- nothing'));

/* the cursor */
$sq4res = $sq4db->query('SELECT a, b, c, d, e FROM q WHERE a < 3 ORDER BY a');
$sq4show('numColumns', fn () => $sq4res->numColumns());
$sq4show('columnName', fn () => [$sq4res->columnName(0), $sq4res->columnName(4),
                                 $sq4res->columnName(5), $sq4res->columnName(-1)]);
$sq4show('columnType with no row up', fn () => [$sq4res->columnType(0), $sq4res->columnType(3)]);
$sq4show('fetchArray both', fn () => $sq4res->fetchArray());
$sq4show('columnType with a row up', fn () => [$sq4res->columnType(0), $sq4res->columnType(1),
                                               $sq4res->columnType(2), $sq4res->columnType(3),
                                               $sq4res->columnType(4)]);
/* a column that is not there is a NULL rather than a refusal -- and asking
   leaves an error on the CONNECTION, which columnName() never does */
$sq4show('columnType out of range', function () use ($sq4db, $sq4res) {
    $t = [$sq4res->columnType(5), $sq4res->columnType(-1)];
    return [$t, $sq4db->lastErrorCode(), $sq4db->lastErrorMsg()]; });
$sq4show('fetchArray assoc', fn () => $sq4res->fetchArray(SQLITE3_ASSOC));
$sq4show('past the end', fn () => $sq4res->fetchArray());
$sq4show('and the walk STARTS OVER', fn () => $sq4res->fetchArray(SQLITE3_NUM));
$sq4show('reset', fn () => $sq4res->reset());
$sq4show('after reset', fn () => $sq4res->fetchArray(SQLITE3_NUM));
$sq4show('a mode is two bits', fn () => $sq4res->fetchArray(99));
$sq4show('mode zero', function () use ($sq4db) {
    return $sq4db->query('SELECT 1 AS one')->fetchArray(0); });

/* fetchAll: what is AHEAD, and a rewind at the end */
$sq4show('fetchAll', fn () => $sq4db->query('SELECT a FROM q WHERE a < 3 ORDER BY a')
                                    ->fetchAll(SQLITE3_NUM));
$sq4show('fetchAll of nothing', fn () => $sq4db->query('SELECT a FROM q WHERE a = 99')->fetchAll());
$sq4show('fetchAll after a fetch', function () use ($sq4db) {
    $r = $sq4db->query('SELECT a FROM q WHERE a < 4 ORDER BY a');
    $r->fetchArray();
    return $r->fetchAll(SQLITE3_NUM); });
$sq4show('fetchAll twice', function () use ($sq4db) {
    $r = $sq4db->query('SELECT a FROM q WHERE a < 3 ORDER BY a');
    return [count($r->fetchAll()), count($r->fetchAll())]; });

/* a result over a write */
$sq4show('a write has no columns', function () use ($sq4db) {
    $r = $sq4db->query("UPDATE q SET b='k' WHERE a = 99");
    return [$r->numColumns(), $r->fetchArray()]; });

/* finalize, and what it leaves behind */
$sq4show('finalize', fn () => $sq4res->finalize());
$sq4show('fetch after finalize', fn () => $sq4res->fetchArray());
$sq4show('numColumns after finalize', fn () => $sq4res->numColumns());
$sq4show('finalize twice', fn () => $sq4res->finalize());
$sq4show('the result outlives the db', function () {
    $d = new SQLite3(':memory:');
    $d->exec('CREATE TABLE u (a)');
    $d->exec('INSERT INTO u VALUES (7)');
    $r = $d->query('SELECT a FROM u');
    unset($d);
    return $r->fetchArray(SQLITE3_NUM); });
$sq4show('a closed db has no query', function () {
    $d = new SQLite3(':memory:'); $d->close(); return $d->query('SELECT 1'); });
$sq4show('a blank result', function () {
    return (new ReflectionClass('SQLite3Result'))->newInstanceWithoutConstructor()->numColumns(); });
$sq4show('new SQLite3Result', fn () => new SQLite3Result());
$sq4show('clone a result', fn () => clone $sq4db->query('SELECT 1'));
$sq4show('serialize a result', fn () => serialize($sq4db->query('SELECT 1')));
$sq4show('a result shows nothing', function () use ($sq4db) {
    ob_start(); var_dump($sq4db->query('SELECT 1'));
    return preg_replace('/#\d+/', '#N', ob_get_clean()); });

/* querySingle: one step, no cursor, and two shapes of "no row" */
$sq4show('querySingle a scalar', fn () => $sq4db->querySingle('SELECT b FROM q WHERE a = 1'));
$sq4show('querySingle a row', fn () => $sq4db->querySingle('SELECT a, b FROM q WHERE a = 1', true));
$sq4show('querySingle no row', fn () => $sq4db->querySingle('SELECT b FROM q WHERE a = 99'));
$sq4show('querySingle no row, whole', fn () => $sq4db->querySingle('SELECT a FROM q WHERE a = 99', true));
$sq4show('querySingle a write', fn () => $sq4db->querySingle('DELETE FROM q WHERE a = 99'));
$sq4show('querySingle broken', fn () => $sq4db->querySingle('NOT SQL'));
$sq4show('querySingle empty', fn () => $sq4db->querySingle(''));
$sq4show('querySingle a comment', fn () => $sq4db->querySingle('-- nothing'));
$sq4show('querySingle first column only', fn () => $sq4db->querySingle('SELECT 7, 8'));

/* close() is not a release: php cleans the list of everything the connection
   handed out, so a result walking one of its statements is finished too */
$sq4show('a walk cut off by close()', function () {
    $d = new SQLite3(':memory:');
    $d->exec('CREATE TABLE t (a)');
    $d->exec('INSERT INTO t VALUES (1),(2)');
    $r = $d->query('SELECT a FROM t ORDER BY a');
    $first = $r->fetchArray(SQLITE3_NUM);
    $d->close();
    return [$first, $r->fetchArray(SQLITE3_NUM)]; });
$sq4show('numColumns after close()', function () {
    $d = new SQLite3(':memory:'); $r = $d->query('SELECT 1, 2'); $d->close();
    return $r->numColumns(); });
$sq4show('a reopen does not revive it', function () {
    $d = new SQLite3(':memory:');
    $d->exec('CREATE TABLE t (a)');
    $d->exec('INSERT INTO t VALUES (5)');
    $r = $d->query('SELECT a FROM t');
    $d->close();
    $d->open(':memory:');
    return [$r->fetchArray(SQLITE3_NUM), $d->exec('CREATE TABLE u (a)')]; });
--EXPECT--
query a select                     => obj:SQLite3Result
query a write RAN it               => 3
and its result RUNS IT AGAIN       => [0=>4, 1=>false, 2=>5]
only the FIRST statement           => [0=>1]
  [2] SQLite3::query(): Unable to prepare statement: near "NOT": syntax error
query broken                       => false
  [2] SQLite3::query(): Unable to execute statement: integer overflow
query that fails at the STEP       => false
query empty                        => false
  [2] SQLite3::query(): Unable to execute statement: not an error
query a comment                    => false
numColumns                         => 5
columnName                         => [0=>"a", 1=>"e", 2=>false, 3=>false]
columnType with no row up          => [0=>false, 1=>false]
fetchArray both                    => [0=>1, 'a'=>1, 1=>"x", 'b'=>"x", 2=>1.5, 'c'=>1.5, 3=>hex:00ff, 'd'=>hex:00ff, 4=>NULL, 'e'=>NULL]
columnType with a row up           => [0=>1, 1=>3, 2=>2, 3=>4, 4=>5]
columnType out of range            => [0=>[0=>5, 1=>5], 1=>25, 2=>"column index out of range"]
fetchArray assoc                   => ['a'=>2, 'b'=>"y", 'c'=>2.5, 'd'=>hex:0102, 'e'=>NULL]
past the end                       => false
and the walk STARTS OVER           => [0=>1, 1=>"x", 2=>1.5, 3=>hex:00ff, 4=>NULL]
reset                              => true
after reset                        => [0=>1, 1=>"x", 2=>1.5, 3=>hex:00ff, 4=>NULL]
a mode is two bits                 => [0=>2, 'a'=>2, 1=>"y", 'b'=>"y", 2=>2.5, 'c'=>2.5, 3=>hex:0102, 'd'=>hex:0102, 4=>NULL, 'e'=>NULL]
mode zero                          => []
fetchAll                           => [0=>[0=>1], 1=>[0=>2]]
fetchAll of nothing                => []
fetchAll after a fetch             => [0=>[0=>2], 1=>[0=>3]]
fetchAll twice                     => [0=>2, 1=>2]
a write has no columns             => [0=>0, 1=>false]
finalize                           => true
fetch after finalize               => Error: The SQLite3Result object has not been correctly initialised or is already closed
numColumns after finalize          => Error: The SQLite3Result object has not been correctly initialised or is already closed
finalize twice                     => Error: The SQLite3Result object has not been correctly initialised or is already closed
the result outlives the db         => [0=>7]
a closed db has no query           => Error: The SQLite3 object has not been correctly initialised or is already closed
a blank result                     => Error: The SQLite3Result object has not been correctly initialised or is already closed
new SQLite3Result                  => Error: Call to private SQLite3Result::__construct() from global scope
clone a result                     => Error: Trying to clone an uncloneable object of class SQLite3Result
serialize a result                 => Exception: Serialization of 'SQLite3Result' is not allowed
a result shows nothing             => hex:6f626a6563742853514c69746533526573756c7429234e20283029207b0a7d0a
querySingle a scalar               => "x"
querySingle a row                  => ['a'=>1, 'b'=>"x"]
querySingle no row                 => NULL
querySingle no row, whole          => []
querySingle a write                => NULL
  [2] SQLite3::querySingle(): Unable to prepare statement: near "NOT": syntax error
querySingle broken                 => false
querySingle empty                  => false
  [2] SQLite3::querySingle(): Unable to execute statement: not an error
querySingle a comment              => false
querySingle first column only      => 7
a walk cut off by close()          => Error: The SQLite3Result object has not been correctly initialised or is already closed
numColumns after close()           => Error: The SQLite3Result object has not been correctly initialised or is already closed
a reopen does not revive it        => Error: The SQLite3Result object has not been correctly initialised or is already closed
