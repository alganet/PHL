--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_starts_with rejects prefix that diverges mid-needle
--FILE--
<?php
echo "diverge_last="  . (str_starts_with("abcdef", "abcx") ? 'true' : 'false') . "\n";
echo "diverge_mid="   . (str_starts_with("abcdef", "abXdef") ? 'true' : 'false') . "\n";
echo "exact_prefix="  . (str_starts_with("abcdef", "abc") ? 'true' : 'false') . "\n";
?>
--EXPECT--
diverge_last=false
diverge_mid=false
exact_prefix=true
--CLEAN--
<?php

