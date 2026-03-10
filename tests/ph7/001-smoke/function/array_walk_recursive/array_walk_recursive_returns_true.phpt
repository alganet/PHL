--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive returns true on success
--FILE--
<?php
$in = array('a' => array(1, 2), 'b' => 3);
$result = array_walk_recursive($in, function($v, $k) {});
echo $result ? 'true' : 'false';
?>
--EXPECT--
true
--CLEAN--
<?php
unset($in, $result);
