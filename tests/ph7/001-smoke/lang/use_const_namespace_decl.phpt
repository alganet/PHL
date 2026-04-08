--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use const imports a constant declared with const in another namespace
--FILE--
<?php
namespace UseConstDeclNs;
const UCD_VAL = 99;

namespace UseConstDeclApp;
use const UseConstDeclNs\UCD_VAL;
echo UCD_VAL . "\n";
echo "done\n";
?>
--EXPECT--
99
done
--CLEAN--
<?php

