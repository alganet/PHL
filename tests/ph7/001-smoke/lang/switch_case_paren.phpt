--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
switch statement with case expression in parentheses
--FILE--
<?php
$x = 3;
switch ($x) {
    case (1 + 2):
        echo "matched\n";
        break;
    default:
        echo "not matched\n";
}
?>
--EXPECT--
matched
--CLEAN--
<?php
unset($x);
