--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chr(65) returns "A"
--FILE--
<?php
echo chr(65) . "\n";
?>
--EXPECT--
A
--CLEAN--
<?php

