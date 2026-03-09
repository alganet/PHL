--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map works with single element array
--FILE--
<?php
$r = array_map(function($v) { return $v + 1; }, array(5));
echo $r[0];
?>
--EXPECT--
6
--CLEAN--
<?php
unset($r);
