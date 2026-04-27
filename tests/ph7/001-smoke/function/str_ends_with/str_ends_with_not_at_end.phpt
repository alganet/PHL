--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ends_with rejects substring not at end
--FILE--
<?php
echo "middle=" . (str_ends_with("Hello World", "Worl") ? 'true' : 'false') . "\n";
echo "start="  . (str_ends_with("Hello World", "Hello") ? 'true' : 'false') . "\n";
?>
--EXPECT--
middle=false
start=false
--CLEAN--
<?php

