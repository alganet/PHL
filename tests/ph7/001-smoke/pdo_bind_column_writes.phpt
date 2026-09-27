--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A bound column is written by EVERY fetch, not only by PDO::FETCH_BOUND
--DESCRIPTION--
`bindColumn()` attaches a variable to a column and php writes it from every row
any verb reads — fetch() in any mode, fetchAll(), fetchColumn(), fetchObject()
and a foreach alike. FETCH_BOUND is only the mode that answers `true` INSTEAD of
a row; it is not the mode that does the writing.

What lands in the variable is decided by the type it was bound with, and php
switches on the value it was GIVEN: PARAM_NULL writes a null whatever the column
holds, INT/STR/BOOL convert, and everything else — PARAM_STMT, a number no
constant names, or any of these with PARAM_INPUT_OUTPUT ored on — writes the
driver's own value untouched. A column holding SQL NULL stays null through all
of them.

A second bindColumn REPLACES rather than stacks, and which it considers the same
is not symmetric: a binding made by NAME takes over whatever stands for that
COLUMN, while one made by NUMBER only replaces another made by number.

A binding naming a column the statement does not have writes a NULL into its own
variable and only then refuses with `Invalid column index`,
and php reads the row and moves the cursor ON before that refusal surfaces: the
bindings it CAN honour are written, three fetches over three rows refuse one by
one and the fourth answers false, while fetchAll() walks the whole set (writing
from every row) and refuses once. A cursor with nothing left never looks at the
bindings at all.
--FILE--
<?php
$bcDb = new PDO('sqlite::memory:');
$bcDb->exec('CREATE TABLE bc (a TEXT, b TEXT)');
$bcDb->exec("INSERT INTO bc VALUES ('1','x'),('2','y'),('3','z')");
$bcMk = function () use ($bcDb) { return $bcDb->query('SELECT * FROM bc'); };

/* every mode writes, and FETCH_BOUND is only the one that answers true */
foreach (['ASSOC' => PDO::FETCH_ASSOC, 'NUM' => PDO::FETCH_NUM, 'BOTH' => PDO::FETCH_BOTH,
          'OBJ' => PDO::FETCH_OBJ, 'NAMED' => PDO::FETCH_NAMED, 'BOUND' => PDO::FETCH_BOUND,
          'LAZY' => PDO::FETCH_LAZY, 'COLUMN' => PDO::FETCH_COLUMN] as $bcName => $bcMode) {
    $bcSt = $bcMk();
    if ($bcMode === PDO::FETCH_COLUMN) { $bcSt->setFetchMode(PDO::FETCH_COLUMN, 1); }
    $bcVar = 'untouched';
    $bcSt->bindColumn(1, $bcVar);
    $bcRow = $bcMode === PDO::FETCH_COLUMN ? $bcSt->fetch() : $bcSt->fetch($bcMode);
    printf("%-6s bound=%s answered=%s\n", $bcName, json_encode($bcVar),
        is_object($bcRow) ? get_class($bcRow) : json_encode($bcRow));
}
foreach (['fetchAll', 'fetchColumn', 'fetchObject'] as $bcVerb) {
    $bcSt = $bcMk();
    $bcVar = 'untouched';
    $bcSt->bindColumn(2, $bcVar);
    $bcSt->$bcVerb();
    printf("%-12s bound=%s\n", $bcVerb, json_encode($bcVar));
}
$bcSt = $bcMk();
$bcSeen = [];
$bcSt->bindColumn(1, $bcWalk);
foreach ($bcSt as $bcRow) { $bcSeen[] = $bcWalk; }
echo 'foreach bound=', json_encode($bcSeen), "\n";

/* the type decides what lands, and a NULL column stays null */
$bcDb->exec('CREATE TABLE bt (s TEXT, i INTEGER, r REAL, n TEXT)');
$bcDb->exec("INSERT INTO bt VALUES ('hello', 7, 2.5, NULL)");
foreach ([
    'NULL' => PDO::PARAM_NULL, 'INT' => PDO::PARAM_INT, 'STR' => PDO::PARAM_STR,
    'BOOL' => PDO::PARAM_BOOL, 'STMT' => PDO::PARAM_STMT, 'unknown' => 99,
    'STR|IO' => PDO::PARAM_STR | PDO::PARAM_INPUT_OUTPUT,
] as $bcTypeName => $bcType) {
    $bcSt = $bcDb->query('SELECT r, n FROM bt');
    $bcVal = 'untouched'; $bcNul = 'untouched';
    $bcSt->bindColumn(1, $bcVal, $bcType);
    $bcSt->bindColumn(2, $bcNul, $bcType);
    $bcSt->fetch();
    printf("%-8s value=%s null-column=%s\n", $bcTypeName, json_encode($bcVal), json_encode($bcNul));
}

/* a second binding REPLACES, and the rule is not symmetric */
$bcPairs = [
    'index twice'      => function ($s, &$x, &$y) { $s->bindColumn(1, $x); $s->bindColumn(1, $y); },
    'name twice'       => function ($s, &$x, &$y) { $s->bindColumn('a', $x); $s->bindColumn('a', $y); },
    'name then index'  => function ($s, &$x, &$y) { $s->bindColumn('a', $x); $s->bindColumn(1, $y); },
    'index then name'  => function ($s, &$x, &$y) { $s->bindColumn(1, $x); $s->bindColumn('a', $y); },
    'two columns'      => function ($s, &$x, &$y) { $s->bindColumn('a', $x); $s->bindColumn('b', $y); },
];
foreach ($bcPairs as $bcWhat => $bcBind) {
    $bcSt = $bcMk();
    $bcX = 'untouched'; $bcY = 'untouched';
    $bcBind($bcSt, $bcX, $bcY);
    $bcSt->fetch();
    printf("%-16s => %s\n", $bcWhat, json_encode([$bcX, $bcY]));
}

/* a column the statement does not have: the row is read and the cursor moves on
 * before the refusal, and the honourable bindings are written all the same */
$bcSt = $bcMk();
$bcGood = 'untouched'; $bcBad = 'untouched';
$bcSt->bindColumn(1, $bcGood);
$bcSt->bindColumn(9, $bcBad);
foreach (['fetch', 'fetch', 'fetch', 'fetch'] as $bcTry) {
    try { echo 'fetch => ', json_encode($bcSt->fetch()), "\n"; }
    catch (Throwable $e) { echo 'fetch !! ', get_class($e), ': ', $e->getMessage(), "\n"; }
    echo '  bound=', json_encode([$bcGood, $bcBad]), "\n";
}
$bcSt = $bcMk();
$bcGood = 'untouched';
$bcSt->bindColumn(1, $bcGood);
$bcSt->bindColumn(9, $bcBad);
try { $bcSt->fetchAll(); } catch (Throwable $e) { echo 'fetchAll !! ', get_class($e), ': ', $e->getMessage(), "\n"; }
echo '  bound=', json_encode($bcGood), ' then fetch=', json_encode($bcSt->fetch()), "\n";
$bcSt = $bcMk();
$bcSt->bindColumn(9, $bcBad);
try { foreach ($bcSt as $bcRow) {} } catch (Throwable $e) { echo 'foreach !! ', get_class($e), ': ', $e->getMessage(), "\n"; }
/* an EMPTY result set never looks at the bindings */
$bcEmpty = $bcDb->query('SELECT * FROM bc WHERE 0');
$bcNever = 'untouched';
$bcEmpty->bindColumn(9, $bcNever);
var_dump($bcEmpty->fetch(), $bcNever);

/* a NAME the statement does not carry is the layer's own refusal, and binds nothing */
$bcSt = $bcMk();
try { $bcSt->bindColumn('nope', $bcNope); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
/* and a column NUMBER below one is refused where it is given */
try { $bcMk()->bindColumn(0, $bcZero); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECT--
ASSOC  bound="1" answered={"a":"1","b":"x"}
NUM    bound="1" answered=["1","x"]
BOTH   bound="1" answered={"a":"1","0":"1","b":"x","1":"x"}
OBJ    bound="1" answered=stdClass
NAMED  bound="1" answered={"a":"1","b":"x"}
BOUND  bound="1" answered=true
LAZY   bound="1" answered=PDORow
COLUMN bound="1" answered="x"
fetchAll     bound="z"
fetchColumn  bound="x"
fetchObject  bound="x"
foreach bound=["1","2","3"]
NULL     value=null null-column=null
INT      value=2 null-column=null
STR      value="2.5" null-column=null
BOOL     value=true null-column=null
STMT     value=2.5 null-column=null
unknown  value=2.5 null-column=null
STR|IO   value=2.5 null-column=null
index twice      => ["untouched","1"]
name twice       => ["untouched","1"]
name then index  => ["1","1"]
index then name  => ["untouched","1"]
two columns      => ["1","x"]
fetch => fetch !! ValueError: Invalid column index
  bound=["1",null]
fetch => fetch !! ValueError: Invalid column index
  bound=["2",null]
fetch => fetch !! ValueError: Invalid column index
  bound=["3",null]
fetch => false
  bound=["3",null]
fetchAll !! ValueError: Invalid column index
  bound="3" then fetch=false
foreach !! ValueError: Invalid column index
bool(false)
string(9) "untouched"
PDOException: SQLSTATE[HY000]: General error: Did not find column name 'nope' in the defined columns; it will not be bound
ValueError: PDOStatement::bindColumn(): Argument #1 ($column) must be greater than or equal to 1
