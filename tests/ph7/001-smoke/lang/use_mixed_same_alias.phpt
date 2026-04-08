--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use and use function with same alias name (no collision)
--FILE--
<?php
namespace UseMixedNs;
class Qux {}
function Qux() { return "func-Bar"; }

namespace UseMixedApp;
use UseMixedNs\Qux;
use function UseMixedNs\Qux;
echo Qux() . "\n";
$obj = new Qux();
echo get_class($obj) . "\n";
echo "done\n";
?>
--EXPECT--
func-Bar
UseMixedNs\Qux
done
--CLEAN--
<?php

