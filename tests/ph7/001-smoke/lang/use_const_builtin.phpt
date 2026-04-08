--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use const imports a built-in constant
--FILE--
<?php
namespace UseConstBuiltinApp;
use const PHP_INT_SIZE;
echo PHP_INT_SIZE . "\n";
echo "done\n";
?>
--EXPECT--
8
done
--CLEAN--
<?php

