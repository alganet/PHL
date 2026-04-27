--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_starts_with rejects substring not at start
--FILE--
<?php
echo "middle=" . (str_starts_with("Hello World", "ello") ? 'true' : 'false') . "\n";
echo "end="    . (str_starts_with("Hello World", "World") ? 'true' : 'false') . "\n";
?>
--EXPECT--
middle=false
end=false
--CLEAN--
<?php

