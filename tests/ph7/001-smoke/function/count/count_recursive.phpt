--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE on nested array
--FILE--
<?php
$a = array(array(1,2), array(3,4,5));
echo count($a, COUNT_RECURSIVE) . "\n";
?>
--EXPECT--
7
--CLEAN--
<?php
unset($a);
