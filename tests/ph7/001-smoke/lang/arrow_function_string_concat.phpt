--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: string concatenation body
--FILE--
<?php
$f = fn($a, $b) => $a . "-" . $b;
echo $f("foo", "bar"), "\n";
?>
--EXPECT--
foo-bar
--CLEAN--
<?php
unset($f);
