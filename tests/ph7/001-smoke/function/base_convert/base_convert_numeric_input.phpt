--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: base_convert with numeric input converts correctly
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
$result = base_convert(10, 10, 16);
echo $result . "\n";
?>
--EXPECT--
a
--CLEAN--
<?php
unset($result);
