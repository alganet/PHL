--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__PHP_Incomplete_Class: the parse continues, and every surface shows the payload
--DESCRIPTION--
An unknown class used to ABANDON the parse (`unserialize('O:3:"Foo":1:{...')`
reported offset 0 where php parses on), because the carrier object php builds
could not exist. It exists now: an unknown or refused class yields an
__PHP_Incomplete_Class instance whose properties are the payload's, keys kept
raw. The engine's own surfaces read it freely — var_dump/print_r annotate the
mangled keys (["bp":"UicPvt":private]), var_export prints the PLAIN names,
json_encode skips the mangled ones, foreach unmangles the key it yields, the
(array) cast and get_object_vars keep the raw bytes — and serialize() writes the
ORIGINAL class back out. A hand-built carrier has no name member: it serializes
as itself with php's own degenerate count (total minus one, floored at zero), so
a payload NAMING the carrier round-trips to the empty form exactly as php's does.
--FILE--
<?php
echo "-- an unknown class no longer abandons the parse\n";
$uicArr = unserialize('a:2:{i:0;O:6:"UicFoo":1:{s:1:"x";i:5;}i:1;s:2:"ok";}');
echo get_class($uicArr[0]), ' then ', var_export($uicArr[1], true), "\n";

echo "-- but a TRUNCATED unknown-class payload still fails at php's offset\n";
set_error_handler(function ($n, $s) { echo "  W: $s\n"; return true; });
var_dump(unserialize('O:6:"UicFoo":1:{'));
restore_error_handler();

echo "-- the carrier's display surfaces\n";
$uicInc = unserialize('O:6:"UicPvt":3:{s:2:"qq";i:3;s:5:"' . "\0*\0" . 'pp";i:2;s:10:"' . "\0UicPvt\0" . 'bp";i:1;}');
ob_start(); var_dump($uicInc); $uicD = trim(ob_get_clean());
echo str_replace("\n", ' ', preg_replace('/#\d+ /', '#N ', $uicD)), "\n";
ob_start(); print_r($uicInc); $uicP = trim(ob_get_clean());
echo str_replace("\n", ' ', $uicP), "\n";
echo str_replace("\n", ' ', var_export($uicInc, true)), "\n";
echo json_encode($uicInc), "\n";
foreach ($uicInc as $uicK => $uicV) { echo $uicK, '=', $uicV, ' '; }
echo "\n";
var_dump(get_object_vars($uicInc) === (array)$uicInc);

echo "-- clone keeps the carrier; loose equality compares the payload\n";
$uicC = clone $uicInc;
echo get_class($uicC), "\n";
var_dump($uicC == $uicInc);

echo "-- a fresh hand-built carrier serializes as itself, empty\n";
var_dump(serialize(new __PHP_Incomplete_Class));
var_dump(class_exists('__PHP_Incomplete_Class'));
var_dump($uicInc instanceof __PHP_Incomplete_Class);

echo "-- a payload NAMING the carrier keeps carrier semantics, no name member\n";
$uicSelf = unserialize('O:22:"__PHP_Incomplete_Class":1:{s:1:"x";i:1;}');
echo json_encode(array_keys((array)$uicSelf)), "\n";
var_dump(serialize($uicSelf));

echo "-- php decides count = total-1 BEFORE the body, which on a carrier that\n";
echo "   never had a name member means: a count of zero writes no body at all,\n";
echo "   and any higher count writes one entry MORE than it declared.\n";
var_dump(serialize(unserialize('O:22:"__PHP_Incomplete_Class":1:{s:1:"x";i:1;}')));
var_dump(serialize(unserialize('O:22:"__PHP_Incomplete_Class":2:{s:1:"x";i:1;s:1:"y";i:2;}')));
var_dump(serialize(unserialize('O:22:"__PHP_Incomplete_Class":3:{s:1:"x";i:1;s:1:"y";i:2;s:1:"z";i:3;}')));
var_dump(serialize(unserialize('O:22:"__PHP_Incomplete_Class":2:{s:27:"__PHP_Incomplete_Class_Name";s:3:"Zed";s:1:"x";i:1;}')));

echo "-- odd payload keys survive the round trip: empty, duplicated, non-string\n";
var_dump(serialize(unserialize('O:3:"Nun":1:{s:0:"";i:7;}')));
var_dump(serialize(unserialize('O:3:"Dup":2:{s:1:"k";i:1;s:1:"k";i:2;}')));
var_dump(serialize(unserialize('O:3:"Num":1:{i:0;s:1:"v";}')));

echo "-- and a refused class nests all the way down\n";
$uicDeep = 'O:1:"A":1:{s:1:"b";O:1:"B":1:{s:1:"c";O:1:"C":0:{}}}';
var_dump(serialize(unserialize($uicDeep, ['allowed_classes' => false])) === $uicDeep);
--EXPECT--
-- an unknown class no longer abandons the parse
__PHP_Incomplete_Class then 'ok'
-- but a TRUNCATED unknown-class payload still fails at php's offset
  W: unserialize(): Error at offset 16 of 16 bytes
bool(false)
-- the carrier's display surfaces
object(__PHP_Incomplete_Class)#N (4) {   ["__PHP_Incomplete_Class_Name"]=>   string(6) "UicPvt"   ["qq"]=>   int(3)   ["pp":protected]=>   int(2)   ["bp":"UicPvt":private]=>   int(1) }
__PHP_Incomplete_Class Object (     [__PHP_Incomplete_Class_Name] => UicPvt     [qq] => 3     [pp:protected] => 2     [bp:UicPvt:private] => 1 )
\__PHP_Incomplete_Class::__set_state(array(    '__PHP_Incomplete_Class_Name' => 'UicPvt',    'qq' => 3,    'pp' => 2,    'bp' => 1, ))
{"__PHP_Incomplete_Class_Name":"UicPvt","qq":3}
__PHP_Incomplete_Class_Name=UicPvt qq=3 pp=2 bp=1 
bool(true)
-- clone keeps the carrier; loose equality compares the payload
__PHP_Incomplete_Class
bool(true)
-- a fresh hand-built carrier serializes as itself, empty
string(34) "O:22:"__PHP_Incomplete_Class":0:{}"
bool(true)
bool(true)
-- a payload NAMING the carrier keeps carrier semantics, no name member
["x"]
string(34) "O:22:"__PHP_Incomplete_Class":0:{}"
-- php decides count = total-1 BEFORE the body, which on a carrier that
   never had a name member means: a count of zero writes no body at all,
   and any higher count writes one entry MORE than it declared.
string(34) "O:22:"__PHP_Incomplete_Class":0:{}"
string(58) "O:22:"__PHP_Incomplete_Class":1:{s:1:"x";i:1;s:1:"y";i:2;}"
string(70) "O:22:"__PHP_Incomplete_Class":2:{s:1:"x";i:1;s:1:"y";i:2;s:1:"z";i:3;}"
string(26) "O:3:"Zed":1:{s:1:"x";i:1;}"
-- odd payload keys survive the round trip: empty, duplicated, non-string
string(25) "O:3:"Nun":1:{s:0:"";i:7;}"
string(26) "O:3:"Dup":1:{s:1:"k";i:2;}"
string(30) "O:3:"Num":1:{s:1:"0";s:1:"v";}"
-- and a refused class nests all the way down
bool(true)
