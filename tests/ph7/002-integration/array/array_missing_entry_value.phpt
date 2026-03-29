--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array with missing entry value
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = array(key => );
?>
--EXPECTF--
%s Fatal error:  array(): Missing entry value %s
--CLEAN--
<?php
unset($a);
