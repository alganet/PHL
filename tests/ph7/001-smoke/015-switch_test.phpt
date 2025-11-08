--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Switch statement
--FILE--
<?php
$value = 2;
switch ($value) {
    case 1:
        echo "one";
        break;
    case 2:
        echo "two";
        break;
    default:
        echo "other";
}
?>
--EXPECT--
two
--CLEAN--
<?php
unset($value);
?>