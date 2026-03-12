--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chr(0) returns a null byte (ord 0)
--FILE--
<?php
echo ord(chr(0)) . "\n";
?>
--EXPECT--
0
--CLEAN--
<?php

