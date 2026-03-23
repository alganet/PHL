--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with zero value
--FILE--
<?php
echo base_convert("0", 10, 16) . "\n";
echo base_convert("0", 16, 10) . "\n";
echo base_convert("0", 2, 10) . "\n";
echo base_convert("0", 10, 2) . "\n";
?>
--EXPECT--
0
0
0
0
--CLEAN--
<?php

