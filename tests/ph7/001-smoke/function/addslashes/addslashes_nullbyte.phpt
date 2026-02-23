--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes escapes NUL byte to "\\0"
--FILE--
<?php
// result should be backslash followed by zero
echo addslashes("\0");
?>
--EXPECT--
\0
--CLEAN--
<?php

