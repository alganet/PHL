--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_starts_with is binary-safe (NUL and UTF-8 bytes)
--FILE--
<?php
echo "utf8="              . (str_starts_with("\xC3\xA9clair", "\xC3\xA9") ? 'true' : 'false') . "\n";
echo "nulbyte="           . (str_starts_with("\x00abc", "\x00") ? 'true' : 'false') . "\n";
echo "match_after_nul="   . (str_starts_with("a\x00Yzz", "a\x00Y") ? 'true' : 'false') . "\n";
echo "diverge_after_nul=" . (str_starts_with("a\x00X", "a\x00Y") ? 'true' : 'false') . "\n";
?>
--EXPECT--
utf8=true
nulbyte=true
match_after_nul=true
diverge_after_nul=false
--CLEAN--
<?php

