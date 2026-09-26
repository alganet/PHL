--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
prepare()/execute(), both placeholder spellings, bindValue vs bindParam, and quote()
--DESCRIPTION--
A prepared statement is the same forward cursor query() answers; execute() is
what rewinds it and takes the first step, so columnCount() and the first
fetch() behave identically whichever verb built it.

The placeholder grammar is sqlite's, reached through php's mapping. A
positional array binds element 0 to parameter 1. A named one accepts the
placeholder with or WITHOUT its colon. A name the statement does not have --
misspelled, extra, or a string key given to a positional statement -- resolves
to index 0, and binding there is sqlite's SQLITE_RANGE: that is why php reports
"column index out of range" for a typo in a placeholder name rather than
anything that names it.

bindValue() and bindParam() differ only in WHEN the value is read: bindValue
copies it now, bindParam remembers the caller's variable and reads it at
execute() -- so a write between the two calls is the value the statement runs
with.

The PARAM_* type decides the CAST, not the value's own type: PARAM_INT over
1.9 binds 1, PARAM_STR over the same binds "1.5", PARAM_NULL binds null
whatever it was handed, and PARAM_LOB binds bytes (NUL included). The one thing
that outranks the declaration is php's own null, which binds as NULL through
any type.
--FILE--
<?php
$db = new PDO('sqlite::memory:');
$db->exec('CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, n INT)');
$db->exec("INSERT INTO t VALUES (1,'a',10),(2,'b',20),(3,'c',30)");
$one = function ($st) { $r = $st->fetch(PDO::FETCH_NUM); return $r === false ? null : $r[0]; };

var_dump(get_class($db->prepare('SELECT name FROM t WHERE id = ?')));
try { $db->prepare('NOT SQL AT ALL'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $db->prepare(''); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* positional: element 0 is parameter 1, and the statement re-runs */
$p = $db->prepare('SELECT name FROM t WHERE id = ?');
var_dump($p->execute([1]), $one($p));
var_dump($p->execute([2]), $one($p));
$two = $db->prepare('SELECT name FROM t WHERE id = ? OR id = ?');
var_dump($two->execute([1, 3]), $one($two), $one($two));

/* every shape that resolves to index 0 is sqlite's range error */
foreach ([['too many', 'SELECT name FROM t WHERE id = ?', [1, 2]],
          ['string key', 'SELECT name FROM t WHERE id = ?', ['x' => 1]],
          ['unknown name', 'SELECT name FROM t WHERE id = :id', [':nope' => 1]],
          ['extra name', 'SELECT name FROM t WHERE id = :id', [':id' => 1, ':x' => 2]]] as $case) {
    try { $db->prepare($case[1])->execute($case[2]); echo $case[0], ": no refusal\n"; }
    catch (Throwable $e) { echo $case[0], ': ', $e->getMessage(), "\n"; }
}

/* named, with and without the colon, and one name used twice */
$n1 = $db->prepare('SELECT name FROM t WHERE id = :id');
var_dump($n1->execute([':id' => 2]), $one($n1));
$n2 = $db->prepare('SELECT name FROM t WHERE id = :id');
var_dump($n2->execute(['id' => 2]), $one($n2));
$n3 = $db->prepare('SELECT name FROM t WHERE id = :id OR n = :id');
var_dump($n3->execute([':id' => 2]), $one($n3));

/* bindValue copies now; bindParam reads at execute */
$bv = $db->prepare('SELECT name FROM t WHERE id = ?');
var_dump($bv->bindValue(1, 2), $bv->execute(), $one($bv));
$v = 1;
$bp = $db->prepare('SELECT name FROM t WHERE id = ?');
$bp->bindParam(1, $v);
$v = 3;
var_dump($bp->execute(), $one($bp));
$bn = $db->prepare('SELECT name FROM t WHERE id = :i');
var_dump($bn->bindValue(':i', 1), $bn->execute(), $one($bn));
try { $db->prepare('SELECT ?')->bindValue(0, 1); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* the type is a cast, and null outranks it */
$typed = $db->prepare('SELECT ? AS a, ? AS b, ? AS c, ? AS d');
$typed->bindValue(1, 5, PDO::PARAM_INT);
$typed->bindValue(2, 'str', PDO::PARAM_STR);
$typed->bindValue(3, null, PDO::PARAM_NULL);
$typed->bindValue(4, true, PDO::PARAM_BOOL);
$typed->execute();
var_dump($typed->fetch(PDO::FETCH_ASSOC));
$casts = [['int on float', 1.9, PDO::PARAM_INT], ['str on float', 1.5, PDO::PARAM_STR],
          ['int on string', '7', PDO::PARAM_INT], ['bool false', false, PDO::PARAM_BOOL],
          ['null through str', null, PDO::PARAM_STR], ['null type on 5', 5, PDO::PARAM_NULL]];
foreach ($casts as $c) {
    $q = $db->prepare('SELECT ? AS v');
    $q->bindValue(1, $c[1], $c[2]);
    $q->execute();
    echo $c[0], ' => ', var_export($one($q), true), "\n";
}
/* an untyped array element is bound as a string */
$u = $db->prepare('SELECT ? AS v');
$u->execute([5]);
var_dump($one($u));
/* a LOB keeps its bytes */
$lob = $db->prepare('SELECT ? AS v');
$lob->bindValue(1, "\0bin", PDO::PARAM_LOB);
$lob->execute();
var_dump(bin2hex($one($lob)));

/* quote */
var_dump($db->quote('plain'), $db->quote("a'b"), $db->quote(''), $db->quote('1', PDO::PARAM_INT));
try { $db->quote("a\0b"); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* a prepared write reports its rows, and a failing one is routed */
$w = $db->prepare('UPDATE t SET name = ? WHERE id = ?');
var_dump($w->execute(['q', 1]), $w->rowCount());
try { $db->prepare('INSERT INTO t (id) VALUES (?)')->execute([1]); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECT--
string(12) "PDOStatement"
PDOException: SQLSTATE[HY000]: General error: 1 near "NOT": syntax error
ValueError: PDO::prepare(): Argument #1 ($query) must not be empty
bool(true)
string(1) "a"
bool(true)
string(1) "b"
bool(true)
string(1) "a"
string(1) "c"
too many: SQLSTATE[HY000]: General error: 25 column index out of range
string key: SQLSTATE[HY000]: General error: 25 column index out of range
unknown name: SQLSTATE[HY000]: General error: 25 column index out of range
extra name: SQLSTATE[HY000]: General error: 25 column index out of range
bool(true)
string(1) "b"
bool(true)
string(1) "b"
bool(true)
string(1) "b"
bool(true)
bool(true)
string(1) "b"
bool(true)
string(1) "c"
bool(true)
bool(true)
string(1) "a"
ValueError: PDOStatement::bindValue(): Argument #1 ($param) must be greater than or equal to 1
array(4) {
  ["a"]=>
  int(5)
  ["b"]=>
  string(3) "str"
  ["c"]=>
  NULL
  ["d"]=>
  int(1)
}
int on float => 1
str on float => '1.5'
int on string => 7
bool false => 0
null through str => NULL
null type on 5 => NULL
string(1) "5"
string(8) "0062696e"
string(7) "'plain'"
string(6) "'a''b'"
string(2) "''"
string(3) "'1'"
PDOException: SQLite PDO::quote does not support null bytes
bool(true)
int(1)
PDOException: SQLSTATE[23000]: Integrity constraint violation: 19 UNIQUE constraint failed: t.id
