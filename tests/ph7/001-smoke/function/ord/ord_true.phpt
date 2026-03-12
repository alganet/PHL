--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ord(true) returns 49
--FILE--
<?php
echo ord(true) . "\n";
?>
--EXPECT--
49
--CLEAN--
<?php

