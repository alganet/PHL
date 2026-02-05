--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: pi() returns numeric value
--FILE--
<?php
// print pi with default precision
$val = pi();
// Ensure it's a number and not false
if (is_float($val) || is_numeric($val)) {
    echo "pi_ok\n";
} else {
    echo "pi_fail\n";
}
?>
--EXPECT--
pi_ok
--CLEAN--
<?php
unset($val);
