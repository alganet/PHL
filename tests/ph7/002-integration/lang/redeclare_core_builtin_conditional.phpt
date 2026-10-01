--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A conditional declaration of a core builtin is refused where the statement sits
--FILE--
<?php
/* Being reached at run time buys the declaration nothing: php refuses it there
 * too. The difference from the unconditional case is only WHEN -- everything
 * ahead of the statement has already run, and the refusal is not catchable. */
echo "ran\n";
try {
    if (true) {
        function array_map($c, $a) { return 42; }
    }
} catch (\Throwable $e) {
    echo "caught ", get_class($e), "\n";
}
echo "unreached\n";
--EXPECTF--
ran
%AFatal error:%ACannot redeclare function array_map()%A
--CLEAN--
<?php
