--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: nullable return type
--FILE--
<?php
$f = fn($x): ?string => $x === 0 ? null : "v" . $x;
echo (is_null($f(0)) ? "NULL" : "not"), "\n";
echo $f(3), "\n";
?>
--EXPECT--
NULL
v3
--CLEAN--
<?php
unset($f);
