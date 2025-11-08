--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Simple function call
--FILE--
<?php
function add($x, $y) {
    return $x + $y;
}
echo add(3, 4);
?>
--EXPECT--
7