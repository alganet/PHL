--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PDO::FETCH_LAZY answers one PDORow per statement, a VIEW of the row the cursor is on
--DESCRIPTION--
PDORow is a fully virtual object: it declares `queryString` and holds NO
property at all, so `get_object_vars()`, `(array)`, `var_export()`,
`json_encode()` and `foreach` all see nothing while `$row->id` reads the column
and `var_dump()`/`print_r()` show every one of them — php's get_properties and
get_debug_info handlers disagreeing on purpose.

There is exactly ONE such object per statement and it is a VIEW: every lazy
fetch answers the same object, and the columns it reads are the ones the cursor
is sitting on, so a later fetch in ANY mode moves it and a fetch that finds
nothing leaves every column null (the NAMES stay — php describes them once).
It keeps the statement alive, which is what lets a row outlive the variable it
was fetched from.

A name is read as a column NUMBER when it is an integer string and as a column
NAME otherwise, byte for byte; `queryString` is answered from the statement
BEFORE any column, so a query selecting a column of that name cannot shadow it —
while `isset()` does not know the name at all, which is php's own asymmetry
between its read and has handlers.

Every write is refused, and the sentence names what was attempted: a property,
an offset, an append, an unset. `??=` writes only when the read answered null,
so it is silent on a column that has a value. The class is final, uncloneable,
unserializable, refuses `new` with a PDOException, and no two rows are
comparable (`$row == $row` is the engine's identity shortcut, not a comparison).
--FILE--
<?php
$lazyDb = new PDO('sqlite::memory:');
$lazyDb->exec('CREATE TABLE lz (id INTEGER, Name TEXT, nul TEXT)');
$lazyDb->exec("INSERT INTO lz VALUES (1,'a',NULL),(2,'b',NULL),(3,'c',NULL)");
$lazySel = 'SELECT id, Name, nul FROM lz';
$lazyOne = function () use ($lazyDb, $lazySel) {
    return $lazyDb->query($lazySel)->fetch(PDO::FETCH_LAZY);
};

/* the declared surface */
$lazyRc = new ReflectionClass('PDORow');
var_dump($lazyRc->isFinal(), $lazyRc->isInternal(), $lazyRc->isInstantiable(),
    $lazyRc->getConstructor(), $lazyRc->getMethods(), $lazyRc->getInterfaceNames());
$lazyRp = new ReflectionProperty('PDORow', 'queryString');
var_dump((string)$lazyRp->getType(), $lazyRp->isPublic(), $lazyRp->hasDefaultValue());
var_dump(get_class_vars('PDORow'));
try { new PDORow(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

$lazyRow = $lazyOne();
echo get_class($lazyRow), "\n";

/* reads: by name, by integer-string name, by offset, and the case rule */
var_dump($lazyRow->id, $lazyRow->Name, $lazyRow->{'0'}, $lazyRow->{'1'});
var_dump($lazyRow->name ?? 'MISS', $lazyRow->NAME ?? 'MISS', $lazyRow->nope ?? 'MISS');
var_dump($lazyRow[0], $lazyRow[1], $lazyRow['Name'], $lazyRow['1'], $lazyRow[true]);
var_dump($lazyRow[-1], $lazyRow[99], $lazyRow[1.9], $lazyRow[null], $lazyRow['nope']);
var_dump($lazyRow->queryString, $lazyRow['queryString']);

/* isset/empty/?? — the value's null-ness decides, and `queryString` is not a
 * name the has handler knows */
var_dump(isset($lazyRow->id), isset($lazyRow->nul), isset($lazyRow->nope),
    isset($lazyRow->queryString), isset($lazyRow[0]), isset($lazyRow[99]),
    isset($lazyRow['queryString']));
var_dump(empty($lazyRow->id), empty($lazyRow->nul), $lazyRow->nul ?? 'dflt');

/* what the object SHOWS, and what it does not */
var_dump(get_object_vars($lazyRow), (array)$lazyRow, json_encode($lazyRow));
var_export($lazyRow); echo "\n";
print_r($lazyRow);
foreach ($lazyRow as $k => $v) { echo "ITERATED $k\n"; }
try { count($lazyRow); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { clone $lazyRow; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { serialize($lazyRow); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { (string)$lazyRow; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump((bool)$lazyRow);

/* every write is refused, and each shape has its own sentence */
$lazyWrite = [
    'prop'      => function ($r) { $r->id = 5; },
    'new prop'  => function ($r) { $r->zz = 5; },
    'prop ++'   => function ($r) { $r->id++; },
    'prop .='   => function ($r) { $r->id .= 'x'; },
    'list'      => function ($r) { [$r->id] = [7]; },
    'unset'     => function ($r) { unset($r->id); },
    'unset qs'  => function ($r) { unset($r->queryString); },
    'offset'    => function ($r) { $r[0] = 5; },
    'offset .=' => function ($r) { $r[0] .= 'x'; },
    'append'    => function ($r) { $r[] = 5; },
    'unset off' => function ($r) { unset($r[0]); },
    'refl set'  => function ($r) { (new ReflectionProperty('PDORow','queryString'))->setValue($r,'x'); },
];
foreach ($lazyWrite as $lazyWhat => $lazyFn) {
    try { $lazyFn($lazyRow); echo "$lazyWhat: WROTE\n"; }
    catch (Throwable $e) { echo "$lazyWhat: ", get_class($e), ': ', $e->getMessage(), "\n"; }
}
/* `??=` reads first: a column with a value is never written */
$lazyRow->id ??= 5;
var_dump($lazyRow->id);
try { $lazyRow->nul ??= 5; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
/* a subscript-write BASE is a read whose value the subscript then refuses */
try { $lazyRow->id[0] = 1; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* property_exists asks the same has handler: a column reports, a NULL one does not */
var_dump(property_exists($lazyRow, 'id'), property_exists($lazyRow, 'nul'),
    property_exists($lazyRow, 'nope'), property_exists($lazyRow, 'queryString'),
    property_exists('PDORow', 'id'));
var_dump((new ReflectionProperty('PDORow','queryString'))->getValue($lazyRow));

/* no two rows compare, and the one true `==` is identity */
$lazyOther = $lazyOne();
var_dump($lazyRow == $lazyRow, $lazyRow === $lazyRow, $lazyRow == $lazyOther,
    $lazyRow < $lazyOther, $lazyRow <=> $lazyOther, $lazyRow == 1, $lazyRow == true);

/* ONE object per statement, and it MOVES with the cursor */
$lazyStmt = $lazyDb->query($lazySel);
$lazyA = $lazyStmt->fetch(PDO::FETCH_LAZY);
$lazyB = $lazyStmt->fetch(PDO::FETCH_LAZY);
var_dump($lazyA === $lazyB, $lazyA->id);
$lazyStmt->fetch(PDO::FETCH_ASSOC);
var_dump($lazyA->id);
var_dump($lazyStmt->fetch(PDO::FETCH_ASSOC), $lazyA->id, $lazyA->queryString);
print_r($lazyA);

/* a row outlives the statement variable, and the connection under it */
$lazyTmp = $lazyDb->query($lazySel);
$lazyKeep = $lazyTmp->fetch(PDO::FETCH_LAZY);
unset($lazyTmp);
var_dump($lazyKeep->id, $lazyKeep->Name);

/* closeCursor() drops the values and keeps the column NAMES */
$lazyClosed = $lazyDb->query($lazySel);
$lazyCr = $lazyClosed->fetch(PDO::FETCH_LAZY);
$lazyClosed->closeCursor();
print_r($lazyCr);
var_dump($lazyCr->id);

/* a column named queryString cannot shadow the statement's own */
$lazyShadow = $lazyDb->query('SELECT Name AS queryString, id FROM lz')->fetch(PDO::FETCH_LAZY);
var_dump($lazyShadow->queryString, $lazyShadow['queryString'], $lazyShadow[0],
    isset($lazyShadow->queryString));
print_r($lazyShadow);

/* two columns of one name: the read takes the FIRST, the presentation the LAST */
$lazyDup = $lazyDb->query('SELECT id AS z, Name AS z FROM lz')->fetch(PDO::FETCH_LAZY);
var_dump($lazyDup->z, $lazyDup[0], $lazyDup[1]);
print_r($lazyDup);

/* isset() and property_exists() ask ONE handler two questions: the first about
 * null-ness, the second — php's non-zero check_empty — about TRUTH. empty()
 * asks the second and never reads the value, which is why `queryString` is
 * empty while reading it works. */
$lazyTruth = $lazyDb->query("SELECT 0 AS z, '0' AS s, '' AS e, ' ' AS b, NULL AS n FROM lz")
    ->fetch(PDO::FETCH_LAZY);
foreach (['z','s','e','b','n','queryString'] as $lazyK) {
    printf("%-12s val=%s isset=%s empty=%s exists=%s coal=%s
", $lazyK,
        json_encode($lazyTruth->$lazyK), json_encode(isset($lazyTruth->$lazyK)),
        json_encode(empty($lazyTruth->$lazyK)), json_encode(property_exists($lazyTruth, $lazyK)),
        json_encode($lazyTruth->$lazyK ?? 'D'));
}

/* ORACLE_NULLS still reshapes the null an EXHAUSTED row reads */
$lazyNulls = new PDO('sqlite::memory:', null, null, [PDO::ATTR_ORACLE_NULLS => PDO::NULL_TO_STRING]);
$lazyNulls->exec('CREATE TABLE o (v INTEGER)');
$lazyNulls->exec('INSERT INTO o VALUES (3)');
$lazyNstmt = $lazyNulls->query('SELECT * FROM o');
$lazyNrow = $lazyNstmt->fetch(PDO::FETCH_LAZY);
var_dump($lazyNrow->v, $lazyNstmt->fetch(PDO::FETCH_ASSOC), $lazyNrow->v, isset($lazyNrow->v));
print_r($lazyNrow);

/* the mode reaches the statement's default and its iterator alike */
$lazyIter = $lazyDb->query($lazySel);
$lazyIter->setFetchMode(PDO::FETCH_LAZY);
$lazySeen = [];
foreach ($lazyIter as $lazyK => $lazyV) {
    $lazySeen[] = [$lazyK, get_class($lazyV), $lazyV->id];
}
print_r($lazySeen);
$lazyDefault = new PDO('sqlite::memory:', null, null, [PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_LAZY]);
$lazyDefault->exec('CREATE TABLE d (a INTEGER)');
$lazyDefault->exec('INSERT INTO d VALUES (7)');
var_dump($lazyDefault->query('SELECT * FROM d')->fetch()->a);

/* fetchAll refuses the mode outright, whether it is given or merely current */
try { $lazyDb->query($lazySel)->fetchAll(PDO::FETCH_LAZY); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$lazyAllStmt = $lazyDb->query($lazySel);
$lazyAllStmt->setFetchMode(PDO::FETCH_LAZY);
try { $lazyAllStmt->fetchAll(); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* a statement with no rows answers false, and a write statement too */
var_dump($lazyDb->query('SELECT * FROM lz WHERE id = 99')->fetch(PDO::FETCH_LAZY));
$lazyIns = $lazyDb->prepare("INSERT INTO lz VALUES (9,'z',NULL)");
$lazyIns->execute();
var_dump($lazyIns->fetch(PDO::FETCH_LAZY));

/* the value modifiers are applied at the READ, not at the fetch */
$lazyMods = new PDO('sqlite::memory:');
$lazyMods->exec('CREATE TABLE m (v INTEGER)');
$lazyMods->exec('INSERT INTO m VALUES (5)');
$lazyMrow = $lazyMods->query('SELECT * FROM m')->fetch(PDO::FETCH_LAZY);
$lazyBefore = $lazyMrow->v;
$lazyMods->setAttribute(PDO::ATTR_STRINGIFY_FETCHES, true);
var_dump($lazyBefore, $lazyMrow->v);
$lazyCase = new PDO('sqlite::memory:', null, null, [PDO::ATTR_CASE => PDO::CASE_UPPER]);
$lazyCase->exec('CREATE TABLE c (Ab INTEGER)');
$lazyCase->exec('INSERT INTO c VALUES (4)');
$lazyCrow = $lazyCase->query('SELECT Ab FROM c')->fetch(PDO::FETCH_LAZY);
var_dump($lazyCrow->AB, $lazyCrow->Ab ?? 'MISS');
print_r($lazyCrow);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
NULL
array(0) {
}
array(0) {
}
string(6) "string"
bool(true)
bool(false)
array(1) {
  ["queryString"]=>
  NULL
}
PDOException: You may not create a PDORow manually
PDORow
int(1)
string(1) "a"
int(1)
string(1) "a"
string(4) "MISS"
string(4) "MISS"
string(4) "MISS"
int(1)
string(1) "a"
string(1) "a"
string(1) "a"
string(1) "a"
NULL
NULL
NULL
NULL
NULL
string(28) "SELECT id, Name, nul FROM lz"
string(28) "SELECT id, Name, nul FROM lz"
bool(true)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)
bool(true)
string(4) "dflt"
array(0) {
}
array(0) {
}
string(2) "{}"
\PDORow::__set_state(array(
))
PDORow Object
(
    [queryString] => SELECT id, Name, nul FROM lz
    [id] => 1
    [Name] => a
    [nul] => 
)
TypeError: count(): Argument #1 ($value) must be of type Countable|array, PDORow given
Error: Trying to clone an uncloneable object of class PDORow
Exception: Serialization of 'PDORow' is not allowed
Error: Object of class PDORow could not be converted to string
bool(true)
prop: Error: Cannot write to PDORow property
new prop: Error: Cannot write to PDORow property
prop ++: Error: Cannot write to PDORow property
prop .=: Error: Cannot write to PDORow property
list: Error: Cannot write to PDORow property
unset: Error: Cannot unset PDORow property
unset qs: Error: Cannot unset PDORow property
offset: Error: Cannot write to PDORow offset
offset .=: Error: Cannot write to PDORow offset
append: Error: Cannot append to PDORow offset
unset off: Error: Cannot unset PDORow offset
refl set: Error: Cannot write to PDORow property
int(1)
Error: Cannot write to PDORow property
Error: Cannot use a scalar value as an array
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
string(28) "SELECT id, Name, nul FROM lz"
bool(true)
bool(true)
bool(false)
bool(false)
int(1)
bool(false)
bool(true)
bool(true)
int(2)
int(3)
bool(false)
NULL
string(28) "SELECT id, Name, nul FROM lz"
PDORow Object
(
    [queryString] => SELECT id, Name, nul FROM lz
    [id] => 
    [Name] => 
    [nul] => 
)
int(1)
string(1) "a"
PDORow Object
(
    [queryString] => SELECT id, Name, nul FROM lz
    [id] => 
    [Name] => 
    [nul] => 
)
NULL
string(38) "SELECT Name AS queryString, id FROM lz"
string(38) "SELECT Name AS queryString, id FROM lz"
string(1) "a"
bool(true)
PDORow Object
(
    [queryString] => SELECT Name AS queryString, id FROM lz
    [id] => 1
)
int(1)
int(1)
string(1) "a"
PDORow Object
(
    [queryString] => SELECT id AS z, Name AS z FROM lz
    [z] => a
)
z            val=0 isset=true empty=true exists=false coal=0
s            val="0" isset=true empty=true exists=false coal="0"
e            val="" isset=true empty=true exists=false coal=""
b            val=" " isset=true empty=false exists=true coal=" "
n            val=null isset=false empty=true exists=false coal="D"
queryString  val="SELECT 0 AS z, '0' AS s, '' AS e, ' ' AS b, NULL AS n FROM lz" isset=false empty=true exists=true coal="SELECT 0 AS z, '0' AS s, '' AS e, ' ' AS b, NULL AS n FROM lz"
int(3)
bool(false)
string(0) ""
bool(true)
PDORow Object
(
    [queryString] => SELECT * FROM o
    [v] => 
)
Array
(
    [0] => Array
        (
            [0] => 0
            [1] => PDORow
            [2] => 1
        )

    [1] => Array
        (
            [0] => 1
            [1] => PDORow
            [2] => 2
        )

    [2] => Array
        (
            [0] => 2
            [1] => PDORow
            [2] => 3
        )

)
int(7)
ValueError: PDOStatement::fetchAll(): Argument #1 ($mode) PDO::FETCH_LAZY cannot be used with PDOStatement::fetchAll()
ValueError: PDOStatement::fetchAll(): Argument #1 ($mode) PDO::FETCH_LAZY cannot be used with PDOStatement::fetchAll()
bool(false)
bool(false)
int(5)
string(1) "5"
int(4)
string(4) "MISS"
PDORow Object
(
    [queryString] => SELECT Ab FROM c
    [AB] => 4
)
