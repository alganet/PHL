--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strcspn returns length before first char from mask
--FILE--
<?php
echo strcspn("abcdef", "cd") . "\n"; // a,b -> 2
?>
--EXPECT--
2
--CLEAN--
<?php

