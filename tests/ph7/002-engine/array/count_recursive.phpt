--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE
--FILE--
<?php
$nested = array(array(1,2), array(3,4,5));
echo count($nested, COUNT_RECURSIVE) . "\n";
?>
--EXPECT--
7