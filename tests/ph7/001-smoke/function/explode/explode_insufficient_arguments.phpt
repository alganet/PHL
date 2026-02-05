--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
explode with insufficient arguments returns false
--FILE--
<?php
$result = explode("a");
echo $result === false ? "EXPLODE_INSUFFICIENT:FALSE\n" : "EXPLODE_INSUFFICIENT:NOT_FALSE\n";
?>
--EXPECT--
EXPLODE_INSUFFICIENT:FALSE
--CLEAN--
<?php
unset($result);
