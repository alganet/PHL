--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
arsort with numeric keys preserves them
--FILE--
<?php
$a = array(3 => 'c', 1 => 'a', 2 => 'b');
arsort($a);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
3: c
2: b
1: a
--CLEAN--
<?php
unset($a);
