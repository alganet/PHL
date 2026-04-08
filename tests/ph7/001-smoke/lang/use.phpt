--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use statement imports namespace
--FILE--
<?php
use MyNamespace\MyClass;
echo "use test\n";
?>
--EXPECT--
use test
--CLEAN--
<?php

