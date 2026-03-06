--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys with strict comparison returns empty when types differ
--FILE--
<?php
$a = array('a' => '1', 'b' => true);
$k = array_keys($a, 1, true);
echo count($k) === 0 ? 'ok' : 'fail';
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($a, $k);
