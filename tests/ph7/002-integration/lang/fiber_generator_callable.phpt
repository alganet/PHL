--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Fiber given a generator-flagged callable must not raise a spurious return TypeError (regression: enforcement gate is VM_FUNC_GENERATOR, not the ctx wrapper linkage)
--FILE--
<?php
function g(): Generator {
    if (false) {
        yield 1;
    }
}
$f = new Fiber('g');
$f->start();
echo "no-fatal\n";
?>
--EXPECT--
no-fatal
--CLEAN--
<?php
