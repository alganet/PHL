--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert coerces numeric types to string
--FILE--
<?php
echo @base_convert(255, 10, 16) . "\n";
echo @base_convert(true, 10, 16) . "\n";
echo @base_convert(false, 10, 16) . "\n";
echo @base_convert(null, 10, 16) . "\n";
echo @base_convert(3.14, 10, 16) . "\n";
?>
--EXPECT--
ff
1
0
0
13a
--CLEAN--
<?php

