--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_starts_with empty haystack with non-empty needle returns false
--FILE--
<?php
echo "match=" . (str_starts_with("", "x") ? 'true' : 'false') . "\n";
?>
--EXPECT--
match=false
--CLEAN--
<?php

