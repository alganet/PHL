--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chr(97) returns "a"
--FILE--
<?php
echo chr(97) . "\n";
?>
--EXPECT--
a
--CLEAN--
<?php

