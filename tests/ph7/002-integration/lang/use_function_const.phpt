--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use function and use const declarations are accepted
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
namespace App;

use function strlen;
use function strtoupper;
use const PHP_EOL;
use const DIRECTORY_SEPARATOR;

echo strlen("hello") . "\n";
echo strtoupper("world") . "\n";
echo "ok\n";
?>
--EXPECT--
5
WORLD
ok
--CLEAN--
<?php

