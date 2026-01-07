--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unary operator ambiguity after ternary colon
--FILE--
<?php
$a = false ? 1 : -5;
echo $a . "\n";
?>
--EXPECT--
-5