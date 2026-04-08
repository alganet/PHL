--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use function with alias
--FILE--
<?php
namespace UseFuncAliasNs;
function ufa_greet() { return "Foo-bar"; }

namespace UseFuncAliasApp;
use function UseFuncAliasNs\ufa_greet as myBar;
echo myBar() . "\n";
echo "done\n";
?>
--EXPECT--
Foo-bar
done
--CLEAN--
<?php

