--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys with strict comparison returns only exact type matches
--FILE--
<?php
$a = array('a' => 1, 'b' => '1', 'c' => true);
$k = array_keys($a, 1, true);
echo implode(',', $k);
?>
--EXPECT--
a
--CLEAN--
<?php
unset($a, $k);
