--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
gettype returns php's long type names (integer/boolean/double/NULL)
--FILE--
<?php
foreach ([42, 1.5, "hello", true, false, [1, 2, 3], null] as $v) {
    echo gettype($v), "\n";
}
$o = new stdClass();
echo gettype($o), "\n";
?>
--EXPECT--
integer
double
string
boolean
boolean
array
NULL
object
--CLEAN--
<?php
