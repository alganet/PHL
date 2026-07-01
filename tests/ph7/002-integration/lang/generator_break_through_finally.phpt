--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break/continue crossing a generator's inline finally runs the finally (ROOT C Face 1)
--FILE--
<?php
function g(){
    for ($i = 0; $i < 3; $i++) {
        try { if ($i == 1) break; yield $i; }
        finally { echo "fin$i\n"; }
    }
    yield 99;
}
foreach (g() as $v) { echo "got $v\n"; }
?>
--EXPECT--
got 0
fin0
fin1
got 99
--CLEAN--
<?php
