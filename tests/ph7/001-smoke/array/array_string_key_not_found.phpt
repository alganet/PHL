--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
array access with non-existent string key
--FILE--
<?php
$a = array('key' => 'value');
echo $a['nonexistent'];
?>
--EXPECT--
--CLEAN--
<?php
unset($a);
