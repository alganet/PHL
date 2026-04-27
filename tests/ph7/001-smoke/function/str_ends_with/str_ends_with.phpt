--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ends_with suffix match
--FILE--
<?php
echo "match=" . (str_ends_with("Hello World", "World") ? 'true' : 'false') . "\n";
?>
--EXPECT--
match=true
--CLEAN--
<?php

