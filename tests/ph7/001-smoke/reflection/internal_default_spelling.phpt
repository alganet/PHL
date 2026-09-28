--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A prelude builtin's default is spelled the way php spells an internal one
--DESCRIPTION--
php has two spellings for the same default and picks by whether the function is
INTERNAL: `null` and double quotes there, `NULL` and single quotes for a
userland one. A builtin written as embedded PHP in the prelude is internal to
php however this engine chose to implement it, and its parameters were being
described through the userland branch -- so scandir()'s `$context = NULL` and
clearstatcache()'s `$filename = ''` came out in the wrong one. XMLWriter's
`toStream()` is the mirror: PHL declared `mixed $stream` where php's arginfo is
the legacy untyped form and ReflectionParameter reports no type at all.
--FILE--
<?php
$idspRows = [['scandir', 2], ['preg_filter', 4], ['preg_replace_callback_array', 3],
             ['clearstatcache', 1], ['glob', 1], ['checkdate', 0], ['tempnam', 1]];
foreach ($idspRows as [$idspName, $idspPos]) {
    echo $idspName, ' #', $idspPos, ': ',
         (string)(new ReflectionFunction($idspName))->getParameters()[$idspPos], "\n";
}
echo 'XMLWriter::toStream #0: ',
     (string)(new ReflectionMethod('XMLWriter', 'toStream'))->getParameters()[0], "\n";

/* A USERLAND function keeps php's other spelling. */
function idspUser($a = null, $b = 'q\'q', $c = "d\"d", $d = [1], $e = 1.5, $f = true) {}
echo new ReflectionFunction('idspUser'), "\n";
--EXPECTF--
scandir #2: Parameter #2 [ <optional> $context = null ]
preg_filter #4: Parameter #4 [ <optional> &$count = null ]
preg_replace_callback_array #3: Parameter #3 [ <optional> &$count = null ]
clearstatcache #1: Parameter #1 [ <optional> string $filename = "" ]
glob #1: Parameter #1 [ <optional> int $flags = 0 ]
checkdate #0: Parameter #0 [ <required> int $month ]
tempnam #1: Parameter #1 [ <required> string $prefix ]
XMLWriter::toStream #0: Parameter #0 [ <required> $stream ]
Function [ <user> function idspUser ] {
  @@ %s %d - %d

  - Parameters [6] {
    Parameter #0 [ <optional> $a = NULL ]
    Parameter #1 [ <optional> $b = 'q'q' ]
    Parameter #2 [ <optional> $c = 'd"d' ]
    Parameter #3 [ <optional> $d = [1] ]
    Parameter #4 [ <optional> $e = 1.5 ]
    Parameter #5 [ <optional> $f = true ]
  }
}
