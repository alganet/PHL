--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
What a PDO and a PDOStatement SHOW, debugDumpParams(), and ATTR_STATEMENT_CLASS
--DESCRIPTION--
A PDO presents NO properties -- var_dump, print_r, the (array) cast,
get_object_vars and json_encode all report an empty object, even though the
connection handle lives on the instance. A PDOStatement presents exactly one,
queryString. Keeping the engine's own storage out of those six surfaces is what
the hidden-slot flag is for.

debugDumpParams() is a diagnostic php PRINTS and does not return (it answers
null). Its bindings appear in the order they were MADE, and the two kinds
report differently: a positional one carries its 0-based paramno with an empty
name, a named one carries paramno -1 and the name WITH its colon.

ATTR_STATEMENT_CLASS replaces PDOStatement for every statement the connection
builds afterwards -- through query() and prepare() alike -- and getAttribute
reports the class that will be built.

This one lives in the process-isolated corpus rather than the smoke set on
purpose: var_dump prints an object's HANDLE, and in the shared in-process
interpreter that number counts every object every earlier test made.
--FILE--
<?php
class PdoPresentationStmt extends PDOStatement { public $extra = 'x'; }

$db = new PDO('sqlite::memory:');
$db->exec('CREATE TABLE t (a INT)');
$db->exec('INSERT INTO t VALUES (1)');

var_dump($db);
print_r($db); echo "\n";
var_dump((array)$db, get_object_vars($db), json_encode($db));

$s = $db->query('SELECT a FROM t');
var_dump($s);
print_r($s); echo "\n";
var_dump((array)$s, get_object_vars($s), json_encode($s));
var_export($s); echo "\n";

$p = $db->prepare('SELECT a FROM t WHERE a = ? AND a <> :n');
$p->bindValue(1, 5, PDO::PARAM_INT);
$p->bindValue(':n', 'str');
$p->debugDumpParams();
$q = $db->prepare('SELECT a FROM t');
$q->execute();
var_dump($q->debugDumpParams());

$db->setAttribute(PDO::ATTR_STATEMENT_CLASS, ['PdoPresentationStmt']);
$s2 = $db->query('SELECT a FROM t');
var_dump(get_class($s2), $s2->extra, $s2->queryString);
var_dump($db->getAttribute(PDO::ATTR_STATEMENT_CLASS));
var_dump(get_class($db->prepare('SELECT a FROM t')));
?>
--EXPECT--
object(PDO)#1 (0) {
}
PDO Object
(
)

array(0) {
}
array(0) {
}
string(2) "{}"
object(PDOStatement)#2 (1) {
  ["queryString"]=>
  string(15) "SELECT a FROM t"
}
PDOStatement Object
(
    [queryString] => SELECT a FROM t
)

array(1) {
  ["queryString"]=>
  string(15) "SELECT a FROM t"
}
array(1) {
  ["queryString"]=>
  string(15) "SELECT a FROM t"
}
string(33) "{"queryString":"SELECT a FROM t"}"
\PDOStatement::__set_state(array(
   'queryString' => 'SELECT a FROM t',
))
SQL: [39] SELECT a FROM t WHERE a = ? AND a <> :n
Params:  2
Key: Position #0:
paramno=0
name=[0] ""
is_param=1
param_type=1
Key: Name: [2] :n
paramno=-1
name=[2] ":n"
is_param=1
param_type=2
SQL: [15] SELECT a FROM t
Params:  0
NULL
string(19) "PdoPresentationStmt"
string(1) "x"
string(15) "SELECT a FROM t"
array(1) {
  [0]=>
  string(19) "PdoPresentationStmt"
}
string(19) "PdoPresentationStmt"
