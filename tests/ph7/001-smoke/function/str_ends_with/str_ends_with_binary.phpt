--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ends_with is binary-safe (NUL and UTF-8 bytes)
--FILE--
<?php
echo "utf8="              . (str_ends_with("caf\xC3\xA9", "\xC3\xA9") ? 'true' : 'false') . "\n";
echo "nulbyte="           . (str_ends_with("abc\x00", "\x00") ? 'true' : 'false') . "\n";
echo "match_after_nul="   . (str_ends_with("zza\x00Y", "a\x00Y") ? 'true' : 'false') . "\n";
echo "diverge_after_nul=" . (str_ends_with("zza\x00X", "a\x00Y") ? 'true' : 'false') . "\n";
?>
--EXPECT--
utf8=true
nulbyte=true
match_after_nul=true
diverge_after_nul=false
--CLEAN--
<?php

