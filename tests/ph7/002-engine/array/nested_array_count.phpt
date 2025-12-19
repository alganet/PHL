--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nested array count recursive
--FILE--
<?php
$a = array(1, array(2, array(3, 4)));
echo count($a, 1) . "\n";
?>
--EXPECT--
6