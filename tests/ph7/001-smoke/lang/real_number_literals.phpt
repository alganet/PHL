--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Real number literals
--FILE--
<?php
echo 3.14 . "\n";
echo 1.0e5 . "\n";
echo 2.5E-3 . "\n";
?>
--EXPECT--
3.14
100000
0.0025
--CLEAN--
<?php

