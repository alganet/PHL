--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trailing comma in single-argument function call
--FILE--
<?php
function tcSaIdentity($x) { return $x; }
echo tcSaIdentity(42,) . "\n";
?>
--EXPECT--
42
--CLEAN--
<?php
