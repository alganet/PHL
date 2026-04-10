--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: no parameters
--FILE--
<?php
$f = fn() => 42;
echo $f(), "\n";
?>
--EXPECT--
42
--CLEAN--
<?php
unset($f);
