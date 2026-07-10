--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert handles arbitrary bases (2..36), not just 2/8/10/16
--FILE--
<?php
echo base_convert("z", 36, 10), "\n";
echo base_convert("zz", 36, 16), "\n";
echo base_convert("100", 10, 3), "\n";
echo base_convert("777", 8, 20), "\n";
echo base_convert("cafe", 16, 36), "\n";
echo base_convert("11011", 2, 10), "\n";
?>
--EXPECT--
35
50f
10201
15b
143i
27
--CLEAN--
<?php
