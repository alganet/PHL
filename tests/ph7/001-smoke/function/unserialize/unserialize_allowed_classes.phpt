--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unserialize() enforces allowed_classes: a refused class becomes __PHP_Incomplete_Class
--DESCRIPTION--
The option was validated and then IGNORED: `['allowed_classes' => false]` still
instantiated every class in the payload and ran its __wakeup()/__unserialize(),
which is exactly what the option exists to prevent. php answers a disallowed (or
unknown) class with an __PHP_Incomplete_Class instance carrying the original
name in __PHP_Incomplete_Class_Name plus the payload's properties as dynamic
ones — keys kept RAW, mangling bytes included — calls no magic method on it, and
re-serializes it back to the ORIGINAL payload byte for byte. The list match is
case-insensitive, an empty list allows nothing, `true` (or no option) allows
everything, a NESTED value inside a refused object still follows its own class's
verdict, an enum E: token bypasses the option entirely, and a name the option
refuses is never handed to the autoloader.
--FILE--
<?php
class UacKept { private $bp = 1; protected $pp = 2; public $qq = 3;
    public function __wakeup() { echo "UAC-WAKEUP\n"; } }
class UacInner { public $v = 1; public function __wakeup() { echo "UAC-INNER-WAKEUP\n"; } }
enum UacSuit { case Hearts; }

$uacPayload = 'O:7:"UacKept":3:{s:11:"' . "\0UacKept\0" . 'bp";i:1;s:5:"' . "\0*\0" . 'pp";i:2;s:2:"qq";i:3;}';

echo "-- false refuses, no wakeup, name + raw keys kept\n";
$uacInc = unserialize($uacPayload, ['allowed_classes' => false]);
echo get_class($uacInc), ' ', gettype($uacInc), "\n";
echo json_encode(array_keys((array)$uacInc)), "\n";

echo "-- and re-serializes back to the ORIGINAL payload\n";
var_dump(serialize($uacInc) === $uacPayload);

echo "-- the list matches case-insensitively; everyone else is refused\n";
$uacBoth = unserialize('a:2:{i:0;O:7:"UacKept":1:{s:2:"qq";i:9;}i:1;O:8:"stdClass":0:{}}',
    ['allowed_classes' => ['uackept']]);
echo get_class($uacBoth[0]), ' / ', get_class($uacBoth[1]), "\n";

echo "-- an empty list allows nothing; true allows everything\n";
echo get_class(unserialize('O:8:"stdClass":0:{}', ['allowed_classes' => []])), "\n";
echo get_class(unserialize('O:8:"stdClass":0:{}', ['allowed_classes' => true])), "\n";

echo "-- a nested ALLOWED object inside a refused one still builds (and wakes)\n";
$uacNest = unserialize('O:8:"UacOuter":1:{s:2:"in";O:8:"UacInner":1:{s:1:"v";i:1;}}',
    ['allowed_classes' => ['UacInner']]);
$uacNestArr = (array)$uacNest;
echo get_class($uacNest), ' holds ', get_class($uacNestArr['in']), "\n";

echo "-- an enum case bypasses the option (php restores the singleton)\n";
var_dump(unserialize(serialize(UacSuit::Hearts), ['allowed_classes' => false]) === UacSuit::Hearts);

echo "-- a refused name never reaches the autoloader\n";
spl_autoload_register($uacLoader = function ($c) { echo "UAC-AUTOLOAD:$c\n"; });
echo get_class(unserialize('O:10:"UacMissing":0:{}', ['allowed_classes' => false])), "\n";
echo "-- an allowed unknown name does\n";
echo get_class(unserialize('O:11:"UacMissing2":0:{}', ['allowed_classes' => ['UacMissing2']])), "\n";
spl_autoload_unregister($uacLoader);
--EXPECT--
-- false refuses, no wakeup, name + raw keys kept
__PHP_Incomplete_Class object
["__PHP_Incomplete_Class_Name","\u0000UacKept\u0000bp","\u0000*\u0000pp","qq"]
-- and re-serializes back to the ORIGINAL payload
bool(true)
-- the list matches case-insensitively; everyone else is refused
UAC-WAKEUP
UacKept / __PHP_Incomplete_Class
-- an empty list allows nothing; true allows everything
__PHP_Incomplete_Class
stdClass
-- a nested ALLOWED object inside a refused one still builds (and wakes)
UAC-INNER-WAKEUP
__PHP_Incomplete_Class holds UacInner
-- an enum case bypasses the option (php restores the singleton)
bool(true)
-- a refused name never reaches the autoloader
__PHP_Incomplete_Class
-- an allowed unknown name does
UAC-AUTOLOAD:UacMissing2
__PHP_Incomplete_Class
