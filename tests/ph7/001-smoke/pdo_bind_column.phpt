--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bindColumn() attaches a variable to a column, written on every FETCH_BOUND fetch
--DESCRIPTION--
A bound column is the mirror of a bound parameter: the same record read the
other way, holding the caller's variable and writing to it once per fetch
instead of reading from it once per execute. FETCH_BOUND therefore returns
nothing but true -- the row went into the variables.

The value takes the BOUND type, not the column's, so an unqualified binding
hands back a string where the row itself would have held an int.

The two mistakes are refused at different moments and by different rules. A
NAME is resolved when it is bound, so one the result set does not have is
reported immediately -- routed through the error mode like any layer refusal --
and simply not bound. An out-of-range INDEX is accepted there and refused by
the FETCH, as a ValueError.
--FILE--
<?php
$db = new PDO('sqlite::memory:');
$db->exec('CREATE TABLE t (id INTEGER, name TEXT)');
$db->exec("INSERT INTO t VALUES (1,'a'),(2,'b')");
$q = fn () => $db->query('SELECT id, name FROM t ORDER BY rowid');
$show = function ($label, $fn) {
    try { echo str_pad($label, 24), ' => ', json_encode($fn()), "\n"; }
    catch (Throwable $e) { echo str_pad($label, 24), ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
};

$show('by index', function () use ($q) {
    $s = $q(); $s->bindColumn(1, $a); $s->bindColumn(2, $b);
    $r = $s->fetch(PDO::FETCH_BOUND);
    return [$r, $a, $b]; });
$show('by name', function () use ($q) {
    $s = $q(); $s->bindColumn('name', $n); $s->fetch(PDO::FETCH_BOUND); return $n; });
$show('typed int', function () use ($q) {
    $s = $q(); $s->bindColumn(1, $a, PDO::PARAM_INT); $s->fetch(PDO::FETCH_BOUND); return $a; });
$show('per row', function () use ($q) {
    $s = $q(); $s->bindColumn(2, $v); $out = [];
    while ($s->fetch(PDO::FETCH_BOUND)) { $out[] = $v; }
    return $out; });
$show('unknown name', function () use ($q) {
    $s = $q(); $r = $s->bindColumn('nope', $x); $s->fetch(PDO::FETCH_BOUND); return [$r, $x]; });
$show('unknown name silent', function () use ($db) {
    $d = new PDO('sqlite::memory:', null, null, [PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
    $d->exec('CREATE TABLE u (a INT)');
    $d->exec('INSERT INTO u VALUES (7)');
    $s = $d->query('SELECT a FROM u');
    set_error_handler(function ($no, $str) { echo "warning[$no] $str\n"; return true; });
    $r = $s->bindColumn('nope', $x);
    restore_error_handler();
    return [$r, $s->fetch(PDO::FETCH_BOUND), $x]; });
$show('index 0',  function () use ($q) { $s = $q(); return $s->bindColumn(0, $z); });
$show('index 9',  function () use ($q) { $s = $q(); $s->bindColumn(9, $z); return $s->fetch(PDO::FETCH_BOUND); });
$show('fetchAll BOUND', function () use ($q) {
    $s = $q(); $s->bindColumn(2, $w); $r = $s->fetchAll(PDO::FETCH_BOUND); return [$r, $w]; });
?>
--EXPECT--
by index                 => [true,"1","a"]
by name                  => "a"
typed int                => 1
per row                  => ["a","b"]
unknown name             => unknown name             => PDOException: SQLSTATE[HY000]: General error: Did not find column name 'nope' in the defined columns; it will not be bound
unknown name silent      => warning[2] PDOStatement::bindColumn(): SQLSTATE[HY000]: General error: Did not find column name 'nope' in the defined columns; it will not be bound
[true,true,null]
index 0                  => index 0                  => ValueError: PDOStatement::bindColumn(): Argument #1 ($column) must be greater than or equal to 1
index 9                  => index 9                  => ValueError: Invalid column index
fetchAll BOUND           => [[true,true],"b"]
