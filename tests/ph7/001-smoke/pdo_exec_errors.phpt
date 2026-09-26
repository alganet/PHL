--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PDO::exec() answers sqlite's change count, and one failure reaches three error modes
--DESCRIPTION--
exec() runs EVERY statement its string holds and answers sqlite's change
counter -- which a SELECT, a CREATE, a comment or a statement that matches
nothing leaves untouched, so exec() over any of them answers whatever the
previous write answered rather than 0. php reads the same counter, so the
surprise is shared.

errorCode()/errorInfo() have three states, not two: NULL and an EMPTY first
cell before anything has run, "00000" after a success, and the SQLSTATE after a
failure. The failure is invisible again as soon as any other verb runs, because
php clears the handle's error at the ENTRY of exec, query, prepare, quote,
lastInsertId and both attribute accessors -- but not in the two reporters
themselves.

The SQLSTATE decides the sentence: a constraint violation reads "Integrity
constraint violation", not "General error". The driver code beside it is
sqlite's PRIMARY code (19 for any constraint), not the extended one that names
which constraint -- that needs Pdo\Sqlite::ATTR_EXTENDED_RESULT_CODES.
--FILE--
<?php
$db = new PDO('sqlite::memory:');
var_dump($db->errorCode(), $db->errorInfo());

var_dump($db->exec('CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT)'));
var_dump($db->exec("INSERT INTO t (v) VALUES ('a')"));
var_dump($db->exec("INSERT INTO t (v) VALUES ('b'),('c')"));
/* the counter is not touched by a read, so this is still the INSERT's 2 */
var_dump($db->exec('SELECT * FROM t'));
var_dump($db->exec("UPDATE t SET v = 'z'"));
var_dump($db->exec('DELETE FROM t WHERE id = 999'));
/* both statements run; the count is the last one's */
var_dump($db->exec("INSERT INTO t (v) VALUES ('m1'); INSERT INTO t (v) VALUES ('m2');"));
var_dump($db->exec('   '), $db->exec('-- nothing'));
var_dump($db->lastInsertId(), $db->errorCode(), $db->errorInfo());

try { $db->exec(''); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* one failure, three modes */
$handler = function ($no, $str) { echo "warning[$no] $str\n"; return true; };
foreach ([PDO::ERRMODE_SILENT, PDO::ERRMODE_WARNING, PDO::ERRMODE_EXCEPTION] as $mode) {
    $d = new PDO('sqlite::memory:', null, null, [PDO::ATTR_ERRMODE => $mode]);
    set_error_handler($handler);
    try { var_dump($d->exec('SELECT bogus')); }
    catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(),
        ' code=', var_export($e->getCode(), true), ' info=', json_encode($e->errorInfo), "\n"; }
    restore_error_handler();
    echo '  after: ', var_export($d->errorCode(), true), ' ', json_encode($d->errorInfo()), "\n";
    /* any other verb clears it; the reporters do not */
    $d->lastInsertId();
    echo '  cleared: ', var_export($d->errorCode(), true), "\n";
}

/* the SQLSTATE chooses the sentence, and the code is sqlite's primary one */
$d = new PDO('sqlite::memory:', null, null, [PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
$d->exec('CREATE TABLE u (id INTEGER PRIMARY KEY, v TEXT UNIQUE)');
$d->exec("INSERT INTO u (v) VALUES ('x')");
var_dump($d->exec("INSERT INTO u (v) VALUES ('x')"));
var_dump($d->errorCode(), $d->errorInfo());
$d->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
try { $d->exec("INSERT INTO u (v) VALUES ('x')"); }
catch (PDOException $e) { echo $e->getMessage(), "\n"; }
try { $d->exec('INSERT INTO nosuchtable VALUES (1)'); }
catch (PDOException $e) { echo $e->getMessage(), "\n"; }

/* a fresh handle reports "0", not false */
var_dump((new PDO('sqlite::memory:'))->lastInsertId());
?>
--EXPECT--
NULL
array(3) {
  [0]=>
  string(0) ""
  [1]=>
  NULL
  [2]=>
  NULL
}
int(0)
int(1)
int(2)
int(2)
int(3)
int(0)
int(1)
int(1)
int(1)
string(1) "5"
string(5) "00000"
array(3) {
  [0]=>
  string(5) "00000"
  [1]=>
  NULL
  [2]=>
  NULL
}
ValueError: PDO::exec(): Argument #1 ($statement) must not be empty
bool(false)
  after: 'HY000' ["HY000",1,"no such column: bogus"]
  cleared: '00000'
warning[2] PDO::exec(): SQLSTATE[HY000]: General error: 1 no such column: bogus
bool(false)
  after: 'HY000' ["HY000",1,"no such column: bogus"]
  cleared: '00000'
PDOException: SQLSTATE[HY000]: General error: 1 no such column: bogus code='HY000' info=["HY000",1,"no such column: bogus"]
  after: 'HY000' ["HY000",1,"no such column: bogus"]
  cleared: '00000'
bool(false)
string(5) "23000"
array(3) {
  [0]=>
  string(5) "23000"
  [1]=>
  int(19)
  [2]=>
  string(29) "UNIQUE constraint failed: u.v"
}
SQLSTATE[23000]: Integrity constraint violation: 19 UNIQUE constraint failed: u.v
SQLSTATE[HY000]: General error: 1 no such table: nosuchtable
string(1) "0"
