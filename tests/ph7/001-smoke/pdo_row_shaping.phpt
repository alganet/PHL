--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ATTR_CASE, STRINGIFY_FETCHES and ORACLE_NULLS reshape a row; FETCH_NAMED collects duplicates
--DESCRIPTION--
Three connection attributes rewrite what a fetch hands back, and each touches
exactly one thing. ATTR_CASE folds the column NAME -- never a value, and never
a positional key, so FETCH_BOTH's numeric half is untouched.
ATTR_STRINGIFY_FETCHES turns every value the driver typed into a string except
a null, which stays null. ATTR_ORACLE_NULLS trades the empty string and null:
NULL_EMPTY_STRING makes an empty string null, NULL_TO_STRING makes a null the
empty string, and neither touches a string that merely looks empty -- a single
space survives both.

FETCH_NAMED exists for one case the other named modes cannot express: two
columns with the same name. It collects them into a LIST under that name,
where FETCH_ASSOC keeps only the last.

getColumnMeta() reports two type keys that answer different questions --
`sqlite:decl_type` is what the SCHEMA declares, `native_type` is the type of
the value in the CURRENT row -- so a TEXT column holding NULL reports both at
once. Asked for a column that does not exist it answers false, and php reports
the last STEP's code while doing so, which is why the refusal talks about a row
being available rather than about the index.
--FILE--
<?php
$mk = function ($attrs = []) {
    $d = new PDO('sqlite::memory:', null, null, $attrs);
    $d->exec('CREATE TABLE t (MixedCol INTEGER, txt TEXT, r REAL, nil TEXT)');
    $d->exec("INSERT INTO t VALUES (1,'a',1.5,NULL)");
    return $d;
};
$row = function ($d, $mode = PDO::FETCH_ASSOC) {
    return $d->query('SELECT MixedCol, txt, r, nil FROM t')->fetch($mode);
};

foreach ([PDO::CASE_NATURAL, PDO::CASE_UPPER, PDO::CASE_LOWER] as $c) {
    $d = $mk([PDO::ATTR_CASE => $c]);
    echo "case $c assoc => ", json_encode($row($d)), "\n";
    echo "case $c both  => ", json_encode($row($d, PDO::FETCH_BOTH)), "\n";
    echo "case $c obj   => ", json_encode($row($d, PDO::FETCH_OBJ)), "\n";
}

var_dump($row($mk([PDO::ATTR_STRINGIFY_FETCHES => true])));

foreach ([PDO::NULL_NATURAL, PDO::NULL_EMPTY_STRING, PDO::NULL_TO_STRING] as $n) {
    $d = $mk([PDO::ATTR_ORACLE_NULLS => $n]);
    echo "nulls $n => ",
        json_encode($d->query("SELECT NULL AS n, '' AS e, ' ' AS s, 0 AS z FROM t")
            ->fetch(PDO::FETCH_ASSOC)), "\n";
}

$d = $mk();
var_dump($d->query('SELECT MixedCol AS c, txt AS c, r AS c FROM t')->fetch(PDO::FETCH_NAMED));
var_dump($d->query('SELECT MixedCol AS c, txt AS c FROM t')->fetch(PDO::FETCH_ASSOC));
var_dump($d->query('SELECT MixedCol AS c, txt AS c FROM t')->fetch(PDO::FETCH_BOTH));

$s = $d->query('SELECT MixedCol, nil FROM t');
var_dump($s->getColumnMeta(0), $s->getColumnMeta(1));
try { $s->getColumnMeta(99); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $s->getColumnMeta(-1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* sqlite has no second rowset; the cursor is left where it was */
$sd = new PDO('sqlite::memory:', null, null, [PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
$sd->exec('CREATE TABLE u (a INT)');
$sd->exec('INSERT INTO u VALUES (1),(2)');
$nr = $sd->query('SELECT a FROM u');
set_error_handler(function ($no, $str) { echo "warning[$no] $str\n"; return true; });
var_dump($nr->nextRowset());
restore_error_handler();
var_dump($sd->errorInfo(), $nr->fetch(PDO::FETCH_NUM));
try { (new PDO('sqlite::memory:'))->query('SELECT 1')->nextRowset(); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECT--
case 0 assoc => {"MixedCol":1,"txt":"a","r":1.5,"nil":null}
case 0 both  => {"MixedCol":1,"0":1,"txt":"a","1":"a","r":1.5,"2":1.5,"nil":null,"3":null}
case 0 obj   => {"MixedCol":1,"txt":"a","r":1.5,"nil":null}
case 1 assoc => {"MIXEDCOL":1,"TXT":"a","R":1.5,"NIL":null}
case 1 both  => {"MIXEDCOL":1,"0":1,"TXT":"a","1":"a","R":1.5,"2":1.5,"NIL":null,"3":null}
case 1 obj   => {"MIXEDCOL":1,"TXT":"a","R":1.5,"NIL":null}
case 2 assoc => {"mixedcol":1,"txt":"a","r":1.5,"nil":null}
case 2 both  => {"mixedcol":1,"0":1,"txt":"a","1":"a","r":1.5,"2":1.5,"nil":null,"3":null}
case 2 obj   => {"mixedcol":1,"txt":"a","r":1.5,"nil":null}
array(4) {
  ["MixedCol"]=>
  string(1) "1"
  ["txt"]=>
  string(1) "a"
  ["r"]=>
  string(3) "1.5"
  ["nil"]=>
  NULL
}
nulls 0 => {"n":null,"e":"","s":" ","z":0}
nulls 1 => {"n":null,"e":null,"s":" ","z":0}
nulls 2 => {"n":"","e":"","s":" ","z":0}
array(1) {
  ["c"]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    string(1) "a"
    [2]=>
    float(1.5)
  }
}
array(1) {
  ["c"]=>
  string(1) "a"
}
array(3) {
  ["c"]=>
  string(1) "a"
  [0]=>
  int(1)
  [1]=>
  string(1) "a"
}
array(8) {
  ["native_type"]=>
  string(7) "integer"
  ["pdo_type"]=>
  int(1)
  ["sqlite:decl_type"]=>
  string(7) "INTEGER"
  ["table"]=>
  string(1) "t"
  ["flags"]=>
  array(0) {
  }
  ["name"]=>
  string(8) "MixedCol"
  ["len"]=>
  int(-1)
  ["precision"]=>
  int(0)
}
array(8) {
  ["native_type"]=>
  string(4) "null"
  ["pdo_type"]=>
  int(0)
  ["sqlite:decl_type"]=>
  string(4) "TEXT"
  ["table"]=>
  string(1) "t"
  ["flags"]=>
  array(0) {
  }
  ["name"]=>
  string(3) "nil"
  ["len"]=>
  int(-1)
  ["precision"]=>
  int(0)
}
PDOException: SQLSTATE[HY000]: General error: 100 another row available
ValueError: PDOStatement::getColumnMeta(): Argument #1 ($column) must be greater than or equal to 0
warning[2] PDOStatement::nextRowset(): SQLSTATE[IM001]: Driver does not support this function: driver does not support multiple rowsets
bool(false)
array(3) {
  [0]=>
  string(5) "00000"
  [1]=>
  NULL
  [2]=>
  NULL
}
array(1) {
  [0]=>
  int(1)
}
PDOException: SQLSTATE[IM001]: Driver does not support this function: driver does not support multiple rowsets
