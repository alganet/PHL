--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
switch with case expressions
--FILE--
<?php
$x = 2;
switch ($x) {
case 1:
    echo "one\n";
    break;
case 2:
    echo "two\n";
    break;
default:
    echo "other\n";
}
?>
--EXPECT--
two
--CLEAN--
<?php
unset($x);
