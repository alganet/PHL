--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys returns empty array for empty input
--FILE--
<?php
$a = array();
$k = array_keys($a);
echo count($k) === 0 ? 'ok' : 'fail';
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($a, $k);
