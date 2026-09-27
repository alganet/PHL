--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A foreach over a statement honours its fetch mode, not only the row shapes
--DESCRIPTION--
`foreach ($stmt as $row)` walks the statement through its CURRENT fetch mode,
whichever that is. The four row shapes (ASSOC, NUM, BOTH, OBJ — and NAMED beside
them) are only the modes that answer an array: FETCH_CLASS builds one object per
row, FETCH_CLASSTYPE takes the class from the first column and drops it from the
row, FETCH_INTO fills and re-answers the ONE object it was given, FETCH_COLUMN
answers that column's value and FETCH_BOUND answers `true` per row with the
values going to the bound variables. FETCH_LAZY answers the statement's single
PDORow, moving with the walk.

The walk is the same forward cursor every verb shares, so a second foreach over
the same statement walks nothing and a fetch() after one answers false.
--FILE--
<?php
class FeRow { public $a; public $b; public $c; function __construct($c = null) { $this->c = $c; } }
$feDb = new PDO('sqlite::memory:');
$feDb->exec('CREATE TABLE fe (a TEXT, b TEXT)');
$feDb->exec("INSERT INTO fe VALUES ('FeRow','x'),('1','y')");
$feShow = function ($v) {
    if (is_object($v)) { return get_class($v) . ' ' . json_encode(get_object_vars($v)); }
    return json_encode($v);
};
$feModes = [
    'ASSOC'     => [PDO::FETCH_ASSOC, []],
    'NUM'       => [PDO::FETCH_NUM, []],
    'BOTH'      => [PDO::FETCH_BOTH, []],
    'OBJ'       => [PDO::FETCH_OBJ, []],
    'NAMED'     => [PDO::FETCH_NAMED, []],
    'COLUMN'    => [PDO::FETCH_COLUMN, [1]],
    'CLASS'     => [PDO::FETCH_CLASS, ['FeRow']],
    'CLASS+args'=> [PDO::FETCH_CLASS, ['FeRow', ['ctor']]],
    'CLASSTYPE' => [PDO::FETCH_CLASS | PDO::FETCH_CLASSTYPE, []],
    'LAZY'      => [PDO::FETCH_LAZY, []],
];
foreach ($feModes as $feName => [$feMode, $feArgs]) {
    $feSt = $feDb->query('SELECT * FROM fe');
    $feSt->setFetchMode($feMode, ...$feArgs);
    $feSeen = [];
    foreach ($feSt as $feKey => $feVal) { $feSeen[] = [$feKey, $feShow($feVal)]; }
    printf("%-10s %s\n", $feName, json_encode($feSeen));
}

/* FETCH_INTO re-answers the object it was given, filled from each row */
$feInto = new FeRow();
$feSt = $feDb->query('SELECT * FROM fe');
$feSt->setFetchMode(PDO::FETCH_INTO, $feInto);
$feSeen = [];
foreach ($feSt as $feVal) { $feSeen[] = [$feVal === $feInto, json_encode(get_object_vars($feVal))]; }
echo 'INTO       ', json_encode($feSeen), "\n";

/* FETCH_BOUND answers true and the values land in the bound variables */
$feSt = $feDb->query('SELECT * FROM fe');
$feSt->bindColumn(2, $feBound);
$feSt->setFetchMode(PDO::FETCH_BOUND);
$feSeen = [];
foreach ($feSt as $feVal) { $feSeen[] = [$feVal, $feBound]; }
echo 'BOUND      ', json_encode($feSeen), "\n";

/* the LAZY row is ONE object moving with the walk */
$feSt = $feDb->query('SELECT * FROM fe');
$feSt->setFetchMode(PDO::FETCH_LAZY);
$feHeld = null;
$feSeen = [];
foreach ($feSt as $feVal) { $feSeen[] = [$feHeld === null || $feHeld === $feVal, $feVal->a]; $feHeld = $feVal; }
echo 'LAZY same  ', json_encode($feSeen), "\n";

/* the cursor is forward-only and shared with every other verb */
$feSt = $feDb->query('SELECT * FROM fe');
$feSt->setFetchMode(PDO::FETCH_NUM);
foreach ($feSt as $feVal) {}
$feSeen = [];
foreach ($feSt as $feVal) { $feSeen[] = $feVal; }
var_dump($feSeen, $feSt->fetch());

/* an empty result set walks nothing in every mode */
foreach ([PDO::FETCH_ASSOC, PDO::FETCH_LAZY, PDO::FETCH_COLUMN, PDO::FETCH_BOUND] as $feMode) {
    $feSt = $feDb->query('SELECT * FROM fe WHERE 0');
    $feSt->setFetchMode($feMode, ...($feMode === PDO::FETCH_COLUMN ? [0] : []));
    $feN = 0;
    foreach ($feSt as $feVal) { $feN++; }
    echo 'empty ', $feMode, ' => ', $feN, "\n";
}
?>
--EXPECT--
ASSOC      [[0,"{\"a\":\"FeRow\",\"b\":\"x\"}"],[1,"{\"a\":\"1\",\"b\":\"y\"}"]]
NUM        [[0,"[\"FeRow\",\"x\"]"],[1,"[\"1\",\"y\"]"]]
BOTH       [[0,"{\"a\":\"FeRow\",\"0\":\"FeRow\",\"b\":\"x\",\"1\":\"x\"}"],[1,"{\"a\":\"1\",\"0\":\"1\",\"b\":\"y\",\"1\":\"y\"}"]]
OBJ        [[0,"stdClass {\"a\":\"FeRow\",\"b\":\"x\"}"],[1,"stdClass {\"a\":\"1\",\"b\":\"y\"}"]]
NAMED      [[0,"{\"a\":\"FeRow\",\"b\":\"x\"}"],[1,"{\"a\":\"1\",\"b\":\"y\"}"]]
COLUMN     [[0,"\"x\""],[1,"\"y\""]]
CLASS      [[0,"FeRow {\"a\":\"FeRow\",\"b\":\"x\",\"c\":null}"],[1,"FeRow {\"a\":\"1\",\"b\":\"y\",\"c\":null}"]]
CLASS+args [[0,"FeRow {\"a\":\"FeRow\",\"b\":\"x\",\"c\":\"ctor\"}"],[1,"FeRow {\"a\":\"1\",\"b\":\"y\",\"c\":\"ctor\"}"]]
CLASSTYPE  [[0,"FeRow {\"a\":null,\"b\":\"x\",\"c\":null}"],[1,"stdClass {\"b\":\"y\"}"]]
LAZY       [[0,"PDORow []"],[1,"PDORow []"]]
INTO       [[true,"{\"a\":\"FeRow\",\"b\":\"x\",\"c\":null}"],[true,"{\"a\":\"1\",\"b\":\"y\",\"c\":null}"]]
BOUND      [[true,"x"],[true,"y"]]
LAZY same  [[true,"FeRow"],[true,"1"]]
array(0) {
}
bool(false)
empty 2 => 0
empty 1 => 0
empty 7 => 0
empty 6 => 0
