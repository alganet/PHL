--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fetchAll()'s shape-changing modes, fetchColumn(), and setFetchMode()
--DESCRIPTION--
Four of fetchAll()'s modes change the shape of the ARRAY rather than the shape
of a row. FETCH_COLUMN reduces each row to one value; FETCH_KEY_PAIR to a key
and a value, and refuses a result set that is not exactly two columns wide;
FETCH_FUNC replaces the row with whatever a callable answers; and GROUP/UNIQUE
take the first column as a key -- GROUP collecting every row under it, UNIQUE
keeping the last.

Dropping that first column leaves php's own asymmetry behind, and it is
reproduced rather than tidied: a FETCH_NUM row is renumbered from 0, while
FETCH_BOTH's numeric half keeps each column's ORIGINAL position, so a grouped
BOTH row is keyed "name" and 1 with no 0 in sight.

php counts arguments per MODE, so the arity refusals name the fetch mode
instead of the method's signature -- and the FETCH_FUNC one is singular in
php's own source ("expects exactly 2 argument"). The three CLASS-only flags
refuse to ride on any other mode, and that refusal names all three whichever
was set.
--FILE--
<?php
$db = new PDO('sqlite::memory:');
$db->exec('CREATE TABLE t (id INTEGER, name TEXT)');
$db->exec("INSERT INTO t VALUES (1,'a'),(2,'b'),(1,'c')");
$q = fn () => $db->query('SELECT id, name FROM t ORDER BY rowid');
$show = function ($label, $fn) {
    try { echo str_pad($label, 26), ' => ', json_encode($fn()), "\n"; }
    catch (Throwable $e) { echo str_pad($label, 26), ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
};

$show('default',      fn () => $q()->fetchAll());
$show('ASSOC',        fn () => $q()->fetchAll(PDO::FETCH_ASSOC));
$show('NUM',          fn () => $q()->fetchAll(PDO::FETCH_NUM));
$show('OBJ',          fn () => $q()->fetchAll(PDO::FETCH_OBJ));
$show('COLUMN',       fn () => $q()->fetchAll(PDO::FETCH_COLUMN));
$show('COLUMN 1',     fn () => $q()->fetchAll(PDO::FETCH_COLUMN, 1));
$show('COLUMN 9',     fn () => $q()->fetchAll(PDO::FETCH_COLUMN, 9));
$show('COLUMN -1',    fn () => $q()->fetchAll(PDO::FETCH_COLUMN, -1));
$show('KEY_PAIR',     fn () => $q()->fetchAll(PDO::FETCH_KEY_PAIR));
$show('KEY_PAIR 1col',fn () => $db->query('SELECT id FROM t')->fetchAll(PDO::FETCH_KEY_PAIR));
$show('KEY_PAIR 3col',fn () => $db->query('SELECT id, name, id FROM t')->fetchAll(PDO::FETCH_KEY_PAIR));
$show('GROUP|ASSOC',  fn () => $q()->fetchAll(PDO::FETCH_GROUP|PDO::FETCH_ASSOC));
$show('GROUP|NUM',    fn () => $q()->fetchAll(PDO::FETCH_GROUP|PDO::FETCH_NUM));
$show('GROUP bare',   fn () => $q()->fetchAll(PDO::FETCH_GROUP));
$show('UNIQUE|ASSOC', fn () => $q()->fetchAll(PDO::FETCH_UNIQUE|PDO::FETCH_ASSOC));
$show('UNIQUE|NUM',   fn () => $q()->fetchAll(PDO::FETCH_UNIQUE|PDO::FETCH_NUM));
$show('FUNC',         fn () => $q()->fetchAll(PDO::FETCH_FUNC, fn ($i, $n) => "$i:$n"));
$show('FUNC unusable',fn () => $q()->fetchAll(PDO::FETCH_FUNC, 'pdo_fetch_all_no_such_fn'));
$show('FUNC no arg',  fn () => $q()->fetchAll(PDO::FETCH_FUNC));
$show('BOUND',        fn () => $q()->fetchAll(PDO::FETCH_BOUND));
$show('flag misuse',  fn () => $q()->fetchAll(999));
$show('extra arg',    fn () => $q()->fetchAll(PDO::FETCH_ASSOC, 1));
$show('exhausted',    function () use ($q) { $s = $q(); $s->fetchAll(); return $s->fetchAll(); });
$show('on a write',   fn () => $db->query('UPDATE t SET name = name')->fetchAll());

$show('fetchColumn',   fn () => $q()->fetchColumn());
$show('fetchColumn 1', fn () => $q()->fetchColumn(1));
$show('fetchColumn 9', fn () => $q()->fetchColumn(9));
$show('fetchColumn -1',fn () => $q()->fetchColumn(-1));
$show('fetchColumn walk', function () use ($q) { $s = $q();
    return [$s->fetchColumn(), $s->fetchColumn(), $s->fetchColumn(), $s->fetchColumn()]; });

$show('setFetchMode NUM',    function () use ($q) { $s = $q(); return [$s->setFetchMode(PDO::FETCH_NUM), $s->fetch()]; });
$show('setFetchMode + All',  function () use ($q) { $s = $q(); $s->setFetchMode(PDO::FETCH_NUM); return $s->fetchAll(); });
$show('setFetchMode COLUMN', function () use ($q) { $s = $q(); return [$s->setFetchMode(PDO::FETCH_COLUMN, 1), $s->fetch()]; });
$show('setFetchMode flags',  function () use ($q) { return $q()->setFetchMode(999); });
$show('setFetchMode arity',  function () use ($q) { return $q()->setFetchMode(PDO::FETCH_COLUMN); });
$show('connection default',  function () {
    $d = new PDO('sqlite::memory:', null, null, [PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC]);
    $d->exec('CREATE TABLE u (a INT)');
    $d->exec('INSERT INTO u VALUES (5)');
    return $d->query('SELECT a FROM u')->fetch();
});
?>
--EXPECT--
default                    => [{"id":1,"0":1,"name":"a","1":"a"},{"id":2,"0":2,"name":"b","1":"b"},{"id":1,"0":1,"name":"c","1":"c"}]
ASSOC                      => [{"id":1,"name":"a"},{"id":2,"name":"b"},{"id":1,"name":"c"}]
NUM                        => [[1,"a"],[2,"b"],[1,"c"]]
OBJ                        => [{"id":1,"name":"a"},{"id":2,"name":"b"},{"id":1,"name":"c"}]
COLUMN                     => [1,2,1]
COLUMN 1                   => ["a","b","c"]
COLUMN 9                   => COLUMN 9                   => ValueError: Invalid column index
COLUMN -1                  => COLUMN -1                  => ValueError: PDOStatement::fetchAll(): Argument #2 must be greater than or equal to 0
KEY_PAIR                   => {"1":"c","2":"b"}
KEY_PAIR 1col              => KEY_PAIR 1col              => PDOException: SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires the result set to contain exactly 2 columns.
KEY_PAIR 3col              => KEY_PAIR 3col              => PDOException: SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires the result set to contain exactly 2 columns.
GROUP|ASSOC                => {"1":[{"name":"a"},{"name":"c"}],"2":[{"name":"b"}]}
GROUP|NUM                  => {"1":[["a"],["c"]],"2":[["b"]]}
GROUP bare                 => {"1":[{"name":"a","1":"a"},{"name":"c","1":"c"}],"2":[{"name":"b","1":"b"}]}
UNIQUE|ASSOC               => {"1":{"name":"c"},"2":{"name":"b"}}
UNIQUE|NUM                 => {"1":["c"],"2":["b"]}
FUNC                       => ["1:a","2:b","1:c"]
FUNC unusable              => FUNC unusable              => TypeError: function "pdo_fetch_all_no_such_fn" not found or invalid function name
FUNC no arg                => FUNC no arg                => ArgumentCountError: PDOStatement::fetchAll() expects exactly 2 argument for PDO::FETCH_FUNC, 1 given
BOUND                      => [true,true,true]
flag misuse                => flag misuse                => ValueError: PDOStatement::fetchAll(): Argument #1 ($mode) cannot use PDO::FETCH_CLASSTYPE, PDO::FETCH_PROPS_LATE, or PDO::FETCH_SERIALIZE fetch flags with a fetch mode other than PDO::FETCH_CLASS
extra arg                  => extra arg                  => ArgumentCountError: PDOStatement::fetchAll() expects exactly 1 argument for the fetch mode provided, 2 given
exhausted                  => []
on a write                 => []
fetchColumn                => 1
fetchColumn 1              => "a"
fetchColumn 9              => fetchColumn 9              => ValueError: Invalid column index
fetchColumn -1             => fetchColumn -1             => ValueError: Column index must be greater than or equal to 0
fetchColumn walk           => [1,2,1,false]
setFetchMode NUM           => [true,[1,"a"]]
setFetchMode + All         => [[1,"a"],[2,"b"],[1,"c"]]
setFetchMode COLUMN        => [true,"a"]
setFetchMode flags         => setFetchMode flags         => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) cannot use PDO::FETCH_CLASSTYPE, PDO::FETCH_PROPS_LATE, or PDO::FETCH_SERIALIZE fetch flags with a fetch mode other than PDO::FETCH_CLASS
setFetchMode arity         => setFetchMode arity         => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch mode provided, 1 given
connection default         => {"a":5}
