--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
::class on non-existent class returns name as string
--FILE--
<?php
echo NonExistentClass::class . "\n";
echo AnotherMissing::class . "\n";
?>
--EXPECT--
NonExistentClass
AnotherMissing
--CLEAN--
<?php
