--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_contains is binary-safe (NUL and UTF-8 bytes)
--FILE--
<?php
echo "utf8="              . (str_contains("caf\xC3\xA9 latte", "\xC3\xA9") ? 'true' : 'false') . "\n";
echo "nulbyte="           . (str_contains("a\x00b\x00c", "\x00b") ? 'true' : 'false') . "\n";
echo "match_after_nul="   . (str_contains("zza\x00Yzz", "a\x00Y") ? 'true' : 'false') . "\n";
echo "diverge_after_nul=" . (str_contains("a\x00X", "a\x00Y") ? 'true' : 'false') . "\n";
?>
--EXPECT--
utf8=true
nulbyte=true
match_after_nul=true
diverge_after_nul=false
--CLEAN--
<?php

