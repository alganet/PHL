--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ends_with rejects suffix that diverges in earlier byte
--FILE--
<?php
echo "diverge_first=" . (str_ends_with("xyzabc", "Xabc") ? 'true' : 'false') . "\n";
echo "diverge_mid="   . (str_ends_with("abcdef", "cdXf") ? 'true' : 'false') . "\n";
echo "exact_suffix="  . (str_ends_with("abcdef", "def")  ? 'true' : 'false') . "\n";
?>
--EXPECT--
diverge_first=false
diverge_mid=false
exact_suffix=true
--CLEAN--
<?php

