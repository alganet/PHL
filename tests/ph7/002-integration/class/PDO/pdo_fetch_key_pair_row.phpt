--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: fetch(PDO::FETCH_KEY_PAIR) answers one key => value pair, where php's own aborts
--DESCRIPTION--
PDO::FETCH_KEY_PAIR is documented for fetchAll(), and php's `fetch()` cannot do
it at all: the value it builds there is one `var_dump()` crashes on and
`json_encode()` refuses, and `PDO::query($sql, PDO::FETCH_KEY_PAIR)` followed by
a fetch aborts the process outright (SIGILL out of pdo.so). §10 does not
reproduce a php defect, and there is no answer to copy, so PHL gives the mode the
meaning it NAMES and fetchAll() builds: the row as a single key => value pair,
one per fetch, with the two-column requirement php checks for fetchAll() applying
here too.

There is no zend half of this pair — php cannot run one without dying.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned: php's own fetch(FETCH_KEY_PAIR) aborts the process";
}
?>
--FILE--
<?php
$db = new PDO('sqlite::memory:');
$db->exec('CREATE TABLE kp (k TEXT, v TEXT)');
$db->exec("INSERT INTO kp VALUES ('a','1'),('b','2')");

$st = $db->query('SELECT * FROM kp');
$st->setFetchMode(PDO::FETCH_KEY_PAIR);
var_dump($st->fetch(), $st->fetch(), $st->fetch());

/* the same mode given to query() */
var_dump($db->query('SELECT * FROM kp', PDO::FETCH_KEY_PAIR)->fetch());

/* fetchAll() builds the whole map from the same rows */
var_dump($db->query('SELECT * FROM kp')->fetchAll(PDO::FETCH_KEY_PAIR));

/* the width php demands for fetchAll() is demanded here too */
try { $db->query('SELECT k FROM kp', PDO::FETCH_KEY_PAIR)->fetch(); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECT--
array(1) {
  ["a"]=>
  string(1) "1"
}
array(1) {
  ["b"]=>
  string(1) "2"
}
bool(false)
array(1) {
  ["a"]=>
  string(1) "1"
}
array(2) {
  ["a"]=>
  string(1) "1"
  ["b"]=>
  string(1) "2"
}
PDOException: SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires the result set to contain exactly 2 columns.
