--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes escapes high ASCII characters using octal
--FILE--
<?php
// \xC8 is decimal 200, octal 310
echo addcslashes("\xC8","\xC8") . "\n";
?>
--EXPECT--
\310
--CLEAN--
<?php

