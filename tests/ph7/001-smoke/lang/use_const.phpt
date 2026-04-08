--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use const imports a namespaced constant
--FILE--
<?php
namespace UseConstNs;
const UC_VAL = 42;

namespace UseConstApp;
use const UseConstNs\UC_VAL;
echo UC_VAL . "\n";
echo "done\n";
?>
--EXPECT--
42
done
--CLEAN--
<?php

