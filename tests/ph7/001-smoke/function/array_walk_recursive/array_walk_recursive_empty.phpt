--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive on empty array returns true
--FILE--
<?php
$a = array();
$result = array_walk_recursive($a, function($v, $k) {});
echo $result ? 'true' : 'false';
?>
--EXPECT--
true
--CLEAN--
<?php
unset($a, $result);
