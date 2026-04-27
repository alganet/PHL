--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ends_with needle longer than haystack returns false
--FILE--
<?php
echo "match=" . (str_ends_with("abc", "abcd") ? 'true' : 'false') . "\n";
?>
--EXPECT--
match=false
--CLEAN--
<?php

