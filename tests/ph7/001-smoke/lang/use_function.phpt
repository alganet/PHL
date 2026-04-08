--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use function imports a namespaced function
--FILE--
<?php
namespace UseFuncNs;
function uf_greet() { return "Foo-bar"; }

namespace UseFuncApp;
use function UseFuncNs\uf_greet;
echo uf_greet() . "\n";
echo "done\n";
?>
--EXPECT--
Foo-bar
done
--CLEAN--
<?php

