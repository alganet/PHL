--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a closure held in an enclosing variable is callable inside a catch block
--FILE--
<?php
$echoArg = function ($a) { return $a; };
$noArg = function () { return "z"; };
try {
    throw new Exception("e");
} catch (Exception $e) {
    echo $echoArg("hi") . "\n";
    echo $noArg() . "\n";
}
?>
--EXPECT--
hi
z
--CLEAN--
<?php
