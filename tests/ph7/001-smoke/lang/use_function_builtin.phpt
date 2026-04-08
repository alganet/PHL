--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use function imports a built-in function
--FILE--
<?php
namespace UseFuncBuiltinApp;
use function strlen;
echo strlen("hello") . "\n";
echo "done\n";
?>
--EXPECT--
5
done
--CLEAN--
<?php

