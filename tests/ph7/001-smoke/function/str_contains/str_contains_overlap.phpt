--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_contains finds overlapping and repeating patterns
--FILE--
<?php
echo "overlap="   . (str_contains("abab",   "aba") ? 'true' : 'false') . "\n";
echo "repeat="    . (str_contains("aaaa",   "aa")  ? 'true' : 'false') . "\n";
echo "near_miss=" . (str_contains("ababab", "abc") ? 'true' : 'false') . "\n";
?>
--EXPECT--
overlap=true
repeat=true
near_miss=false
--CLEAN--
<?php

