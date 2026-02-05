--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Switch fall-through behavior test
--FILE--
<?php
$val = 2;
switch ($val) {
    case 1:
        echo "one\n";
        break;
    case 2:
        echo "two\n";
    case 3:
        echo "three\n";
        break;
    default:
        echo "default\n";
}
?>
--EXPECT--
two
three
--CLEAN--
<?php
unset($val);
