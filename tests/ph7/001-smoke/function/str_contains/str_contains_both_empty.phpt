--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_contains both empty returns true
--FILE--
<?php
echo "match=" . (str_contains("", "") ? 'true' : 'false') . "\n";
?>
--EXPECT--
match=true
--CLEAN--
<?php

