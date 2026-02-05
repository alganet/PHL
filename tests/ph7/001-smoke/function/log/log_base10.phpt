--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: log with base 10 argument
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
echo log(100, 10) . "\n";
echo log(1000, 10) . "\n";
?>
--EXPECT--
2
3
--CLEAN--
<?php

