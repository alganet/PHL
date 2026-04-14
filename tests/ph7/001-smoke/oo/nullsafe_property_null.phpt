--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe property access on null returns null
--FILE--
<?php
$a = null;
$r = $a?->foo;
echo ($r === null ? "yes" : "no"), "\n";
?>
--EXPECT--
yes
--CLEAN--
<?php
unset($a, $r);
