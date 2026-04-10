--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: static prefix (no $this binding)
--FILE--
<?php
$f = static fn($x) => $x + 100;
echo $f(1), "\n";
echo $f(5), "\n";
?>
--EXPECT--
101
105
--CLEAN--
<?php
unset($f);
