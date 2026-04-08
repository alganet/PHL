--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
::class constant returns class name
--FILE--
<?php
class CcbFoo {}
echo CcbFoo::class . "\n";

interface CcbIface {}
echo CcbIface::class . "\n";

trait CcbTrait {}
echo CcbTrait::class . "\n";
?>
--EXPECT--
CcbFoo
CcbIface
CcbTrait
--CLEAN--
<?php
