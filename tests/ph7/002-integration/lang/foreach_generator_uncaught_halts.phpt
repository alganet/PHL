--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An exception escaping a generator body during foreach propagates and halts (regression: the loop must not silently end with execution continuing)
--FILE--
<?php
function t(): Generator {
    yield 1;
    throw new RuntimeException("boom");
}
foreach (t() as $v) {
    echo "v=", $v, "\n";
}
echo "SHOULD-NOT-PRINT\n";
?>
--EXPECTF--
v=1
%AUncaught RuntimeException: boom%Athrown in %s on line %d
--CLEAN--
<?php
