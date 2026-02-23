--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes escapes multiple kinds of characters in one go
--FILE--
<?php
// mix of single quote, double quote and backslash
echo addslashes("a'b\"\\c");
?>
--EXPECT--
a\'b\"\\c
--CLEAN--
<?php

