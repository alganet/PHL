--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk rejects non-integer size values (future-proof strictness)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$in = array(1,2,3,4);
$size = 2.5;
$chunks = array_chunk($in, $size);
echo count($chunks);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_chunk(): Argument #2 ($length) must be of type int, float given in %s
--CLEAN--
<?php
unset($in, $size, $chunks);
