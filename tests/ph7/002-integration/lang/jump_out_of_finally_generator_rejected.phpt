--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break out of a GENERATOR's finally is rejected too — that body compiles through the inline path, and the rule is on the block, not the strategy
--FILE--
<?php
function jogGen() {
    foreach ([1, 2] as $v) {
        try { yield $v; }
        finally { break; }
    }
    yield 99;
}
foreach (jogGen() as $x) { echo "y$x;"; }
echo "|end\n";
?>
--EXPECTF--
%Ajump out of a finally block is disallowed%A
--CLEAN--
<?php
