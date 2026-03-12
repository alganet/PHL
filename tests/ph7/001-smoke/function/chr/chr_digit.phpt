--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chr(49) returns "1"
--FILE--
<?php
echo chr(49) . "\n";
?>
--EXPECT--
1
--CLEAN--
<?php

