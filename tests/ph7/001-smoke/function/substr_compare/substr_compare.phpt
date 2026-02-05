--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_compare compares substrings at offset
--FILE--
<?php
echo substr_compare('abcdef','abc',0,3) . "\n"; // 0
?>
--EXPECT--
0
--CLEAN--
<?php

