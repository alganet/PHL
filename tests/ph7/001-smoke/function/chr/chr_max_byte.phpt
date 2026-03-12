--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chr(255) returns byte with ord 255
--FILE--
<?php
echo ord(chr(255)) . "\n";
?>
--EXPECT--
255
--CLEAN--
<?php

