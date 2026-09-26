--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Pdo\Sqlite's userland callbacks: SQL functions, aggregates, collations and an authorizer
--DESCRIPTION--
These are the verbs that run PHP from inside sqlite's own loop, and they all
share one rule: a throw cannot travel back through the library's C frames, so
the status is PARKED, sqlite is told to stop, and the exception is raised once
it has unwound. What the script sees is its OWN exception out of query(), not a
PDOException about the statement sqlite abandoned.

What a callback RETURNS is mapped like php maps it: null, int and float pass
through, and everything else takes a string CAST -- which is why a bool comes
back as "1" and an array comes back as "Array" with php's conversion warning
behind it.

An aggregate's two halves both receive the running context and the row count as
their first arguments, and the count the finalizer sees is one past the rows
stepped over -- including for an empty group, where step never runs and the
finalizer still reports 1.

The authorizer is asked while sqlite COMPILES, so a denial stops a prepare
rather than a step: a denied SELECT fails at query() and never reaches a fetch.
--FILE--
<?php
$mk = function () {
    $d = Pdo\Sqlite::connect('sqlite::memory:');
    $d->exec('CREATE TABLE t (a INTEGER, b TEXT)');
    $d->exec("INSERT INTO t VALUES (1,'x'),(2,'y'),(3,'z')");
    return $d;
};
$show = function ($label, $fn) {
    try { echo str_pad($label, 26), ' => ', json_encode($fn()), "\n"; }
    catch (Throwable $e) { echo str_pad($label, 26), ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
};
$db = $mk();

$show('createFunction', fn () => $db->createFunction('pdocb_double', fn ($n) => $n * 2));
$show('use it',         fn () => $db->query('SELECT pdocb_double(a) FROM t')->fetchAll(PDO::FETCH_COLUMN));
$show('two args',       function () use ($db) {
    $db->createFunction('pdocb_cat', fn ($a, $b) => "$a-$b", 2);
    return $db->query('SELECT pdocb_cat(a, b) FROM t')->fetchAll(PDO::FETCH_COLUMN); });
$show('returns null',   function () use ($db) {
    $db->createFunction('pdocb_null', fn () => null);
    return $db->query('SELECT pdocb_null()')->fetch(PDO::FETCH_NUM); });
$show('returns float',  function () use ($db) {
    $db->createFunction('pdocb_f', fn () => 1.5);
    return $db->query('SELECT pdocb_f()')->fetch(PDO::FETCH_NUM)[0]; });
$show('returns bool',   function () use ($db) {
    $db->createFunction('pdocb_b', fn () => true);
    return $db->query('SELECT pdocb_b()')->fetch(PDO::FETCH_NUM)[0]; });
$show('throwing body',  function () use ($db) {
    $db->createFunction('pdocb_throw', function () { throw new RuntimeException('boom'); });
    return $db->query('SELECT pdocb_throw()')->fetch(PDO::FETCH_NUM); });
$show('unusable',       fn () => $db->createFunction('pdocb_bad', 'pdocb_no_such_function'));
$show('deterministic',  fn () => $db->createFunction('pdocb_d', fn () => 1, 0, Pdo\Sqlite::DETERMINISTIC));
$show('redefine',       function () use ($db) {
    $db->createFunction('pdocb_double', fn ($n) => $n * 3);
    return $db->query('SELECT pdocb_double(a) FROM t')->fetchAll(PDO::FETCH_COLUMN); });

$show('createAggregate', fn () => $db->createAggregate('pdocb_sum',
    function ($ctx, $rows, $n) { return ($ctx ?? 0) + $n; },
    function ($ctx, $rows) { return "sum=$ctx rows=$rows"; }));
$show('aggregate',       fn () => $db->query('SELECT pdocb_sum(a) FROM t')->fetch(PDO::FETCH_NUM)[0]);
$show('aggregate empty', fn () => $db->query('SELECT pdocb_sum(a) FROM t WHERE a > 99')->fetch(PDO::FETCH_NUM)[0]);

$show('createCollation', fn () => $db->createCollation('pdocb_rev', fn ($x, $y) => strcmp($y, $x)));
$show('collation',       fn () => $db->query('SELECT b FROM t ORDER BY b COLLATE pdocb_rev')->fetchAll(PDO::FETCH_COLUMN));
$show('collation bad',   fn () => $db->createCollation('pdocb_bad2', 'pdocb_no_such_function'));

$show('authorizer denies', function () use ($mk) {
    $d = $mk(); $d->setAuthorizer(fn () => Pdo\Sqlite::DENY);
    try { return $d->query('SELECT a FROM t')->fetchAll(); }
    catch (Throwable $e) { return get_class($e) . ': ' . $e->getMessage(); } });
$show('authorizer ignores', function () use ($mk) {
    $d = $mk(); $d->setAuthorizer(fn () => Pdo\Sqlite::IGNORE);
    return $d->query('SELECT a FROM t')->fetchAll(PDO::FETCH_NUM); });
$show('authorizer cleared', function () use ($mk) {
    $d = $mk(); $d->setAuthorizer(fn () => Pdo\Sqlite::OK);
    $r = $d->query('SELECT count(*) FROM t')->fetch(PDO::FETCH_NUM)[0];
    $d->setAuthorizer(null);
    return [$r, $d->query('SELECT count(*) FROM t')->fetch(PDO::FETCH_NUM)[0]]; });

$show('extended codes', function () use ($mk) {
    $d = $mk();
    $d->exec('CREATE TABLE u (x INTEGER PRIMARY KEY)');
    $d->exec('INSERT INTO u VALUES (1)');
    $d->setAttribute(Pdo\Sqlite::ATTR_EXTENDED_RESULT_CODES, true);
    $d->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);
    $d->exec('INSERT INTO u VALUES (1)');
    return $d->errorInfo(); });
$show('readonly stmt', fn () => $db->query('SELECT 1')->getAttribute(Pdo\Sqlite::ATTR_READONLY_STATEMENT));
$show('busy stmt',     fn () => $db->query('SELECT 1')->getAttribute(Pdo\Sqlite::ATTR_BUSY_STATEMENT));
$show('open flags get',fn () => $db->getAttribute(Pdo\Sqlite::ATTR_OPEN_FLAGS));
$show('open flags set',fn () => Pdo\Sqlite::connect('sqlite::memory:', null, null,
    [Pdo\Sqlite::ATTR_OPEN_FLAGS => Pdo\Sqlite::OPEN_READONLY]) instanceof Pdo\Sqlite);
$show('loadExtension', fn () => $db->loadExtension('pdocb_no_such_extension'));
?>
--EXPECT--
createFunction             => true
use it                     => [2,4,6]
two args                   => ["1-x","2-y","3-z"]
returns null               => [null]
returns float              => 1.5
returns bool               => "1"
throwing body              => throwing body              => RuntimeException: boom
unusable                   => unusable                   => TypeError: Pdo\Sqlite::createFunction(): Argument #2 ($callback) must be a valid callback, function "pdocb_no_such_function" not found or invalid function name
deterministic              => true
redefine                   => [3,6,9]
createAggregate            => true
aggregate                  => "sum=6 rows=4"
aggregate empty            => "sum= rows=1"
createCollation            => true
collation                  => ["z","y","x"]
collation bad              => collation bad              => TypeError: Pdo\Sqlite::createCollation(): Argument #2 ($callback) must be a valid callback, function "pdocb_no_such_function" not found or invalid function name
authorizer denies          => "PDOException: SQLSTATE[HY000]: General error: 23 not authorized"
authorizer ignores         => []
authorizer cleared         => [3,3]
extended codes             => ["HY000",1555,"UNIQUE constraint failed: u.x"]
readonly stmt              => true
busy stmt                  => true
open flags get             => open flags get             => PDOException: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute
open flags set             => true
loadExtension              => loadExtension              => PDOException: Unable to load extension "pdocb_no_such_extension"
