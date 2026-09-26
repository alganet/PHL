--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PDO::query() answers a forward-only PDOStatement, and sqlite's own column types
--DESCRIPTION--
A statement is a FORWARD cursor and nothing more. php's driver steps once when
the statement runs, so columnCount() can answer before anything is fetched and
the row it landed on is the one the first fetch() hands back. Nothing rewinds:
a second foreach over the same statement walks NOTHING rather than repeating
the set, and closeCursor() ends the walk while leaving the object usable.

rowCount() is not the size of a result set -- sqlite cannot know that without
walking it -- so a SELECT answers 0 and only a write reports rows.

The values come back in sqlite's OWN types: an INTEGER is an int, a REAL is a
float, a BLOB is a string of those bytes and NULL is null. That is what makes
this driver's rows unlike a stringifying one's, and it is why FETCH_BOTH shows
each column twice -- once under its name and once under its position.
--FILE--
<?php
$db = new PDO('sqlite::memory:');
$db->exec('CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, score REAL, blob BLOB, nil TEXT)');
$db->exec("INSERT INTO t VALUES (1,'a',1.5,x'00ff',NULL),(2,'b',2.5,x'0102',NULL)");

$st = $db->query('SELECT id, name FROM t ORDER BY id');
var_dump(get_class($st), $st->queryString, $st->columnCount(), $st->rowCount());
var_dump($st->fetch(PDO::FETCH_ASSOC));
var_dump($st->fetch(PDO::FETCH_NUM));
/* the cursor is spent, and stays spent */
var_dump($st->fetch(), $st->fetch());
var_dump($st->closeCursor(), $st->fetch());

/* a bare fetch() takes the connection's default mode, which is FETCH_BOTH */
$both = $db->query('SELECT id, name FROM t WHERE id = 1')->fetch();
var_dump($both);
$obj = $db->query('SELECT id, name FROM t WHERE id = 1')->fetch(PDO::FETCH_OBJ);
var_dump(get_class($obj), $obj->id, $obj->name);

/* a write answers a statement too: no columns, and a row count */
$w = $db->query("UPDATE t SET name = 'z' WHERE id = 1");
var_dump(get_class($w), $w->columnCount(), $w->rowCount(), $w->fetch());

/* iterating consumes the cursor; the second walk has nothing left */
$it = $db->query('SELECT id FROM t ORDER BY id');
foreach ($it as $k => $row) { echo $k, ' => ', json_encode($row), "\n"; }
foreach ($it as $row) { echo "second pass: ", json_encode($row), "\n"; }
var_dump($it instanceof IteratorAggregate, $it instanceof Traversable,
    $it->getIterator() instanceof Iterator);

/* sqlite's types, unchanged */
var_dump($db->query('SELECT id, name, score, blob, nil FROM t WHERE id = 2')->fetch(PDO::FETCH_ASSOC));

/* a failed query is routed like any other failure */
try { $db->query('SELECT nope'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$sd = new PDO('sqlite::memory:', null, null, [PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
var_dump($sd->query('SELECT nope'), $sd->errorCode());
try { $db->query(''); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* a statement reports the connection's error state */
$s2 = $db->query('SELECT 1 AS one');
var_dump($s2->errorCode(), $s2->errorInfo());
?>
--EXPECT--
string(12) "PDOStatement"
string(34) "SELECT id, name FROM t ORDER BY id"
int(2)
int(0)
array(2) {
  ["id"]=>
  int(1)
  ["name"]=>
  string(1) "a"
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  string(1) "b"
}
bool(false)
bool(false)
bool(true)
bool(false)
array(4) {
  ["id"]=>
  int(1)
  [0]=>
  int(1)
  ["name"]=>
  string(1) "a"
  [1]=>
  string(1) "a"
}
string(8) "stdClass"
int(1)
string(1) "a"
string(12) "PDOStatement"
int(0)
int(1)
bool(false)
0 => {"id":1,"0":1}
1 => {"id":2,"0":2}
bool(true)
bool(true)
bool(true)
array(5) {
  ["id"]=>
  int(2)
  ["name"]=>
  string(1) "b"
  ["score"]=>
  float(2.5)
  ["blob"]=>
  string(2) ""
  ["nil"]=>
  NULL
}
PDOException: SQLSTATE[HY000]: General error: 1 no such column: nope
bool(false)
string(5) "HY000"
ValueError: PDO::query(): Argument #1 ($query) must not be empty
string(5) "00000"
array(3) {
  [0]=>
  string(5) "00000"
  [1]=>
  NULL
  [2]=>
  NULL
}
