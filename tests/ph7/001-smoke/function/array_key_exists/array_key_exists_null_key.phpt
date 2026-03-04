--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: null key matches empty-string key
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = array('' => 'val');
echo array_key_exists(null, $a) ? 'true' : 'false';
echo PHP_EOL;
?>
--EXPECTF--
Error [8192]: Using null as the key parameter for array_key_exists() is deprecated, use an empty string instead in %s on line %d
true
--CLEAN--
<?php
unset($a);
