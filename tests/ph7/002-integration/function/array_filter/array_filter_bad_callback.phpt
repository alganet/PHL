--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
array_filter with a non-callable callback returns an empty array
--FILE--
<?php
$result = array_filter(array(1,2,3), "notfunc");
echo count($result) . "\n";
?>
--EXPECT--
0
--CLEAN--
<?php
unset($result);
