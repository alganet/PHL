--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec returns decimal value of hex string
--FILE--
<?php
echo hexdec('a') . "\n"; // 10
?>
--EXPECT--
10
--CLEAN--
<?php

