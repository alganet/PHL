--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes with octal escaping
--FILE--
<?php
echo addcslashes("\x01", "\x00..\x1F") . "\n";
?>
--EXPECT--
\001
--CLEAN--
<?php

