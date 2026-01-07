--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex ternary expression evaluation
--FILE--
<?php
$a = 5;
$b = 10;
$result = $a > 3 ? ($b < 20 ? "nested_true" : "nested_false") : "false";
if ($result === "nested_true") {
    echo "complex_ternary_ok\n";
} else {
    echo "complex_ternary_fail\n";
}
?>
--EXPECT--
complex_ternary_ok