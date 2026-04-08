--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
::class on non-existent class in namespace
--FILE--
<?php
namespace CcnnTest;
echo CcnnSomeClass::class . "\n";
echo \CcnnOther\CcnnPkg\CcnnThing::class . "\n";
?>
--EXPECT--
CcnnTest\CcnnSomeClass
CcnnOther\CcnnPkg\CcnnThing
--CLEAN--
<?php
