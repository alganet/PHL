--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode with no arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = explode();
echo $result === false ? 'EXPLODE_NO_ARGS:PASS' : 'EXPLODE_NO_ARGS:FAIL';
?>
--EXPECT--
EXPLODE_NO_ARGS:PASS
--CLEAN--
<?php
unset($result);
