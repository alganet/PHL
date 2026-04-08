--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use const with alias
--FILE--
<?php
namespace UseConstAliasNs;
const UCA_VAL = 42;

namespace UseConstAliasApp;
use const UseConstAliasNs\UCA_VAL as LOCAL_CONST;
echo LOCAL_CONST . "\n";
echo "done\n";
?>
--EXPECT--
42
done
--CLEAN--
<?php

