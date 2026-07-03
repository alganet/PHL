--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator supertype check resolves use-import aliases (use Generator as Gen)
--FILE--
<?php
use Generator as Gen;
use Iterator as It;
function g(): Gen {
    yield 1;
}
function h(): It {
    yield 2;
}
foreach (g() as $v) {
    echo $v, "\n";
}
foreach (h() as $v) {
    echo $v, "\n";
}
echo "ok\n";
?>
--EXPECT--
1
2
ok
--CLEAN--
<?php
