--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert octal conversions
--FILE--
<?php
echo base_convert('7', 8, 10) . "\n";
echo base_convert('123', 8, 10) . "\n";
?>
--EXPECT--
7
83
--CLEAN--
<?php

