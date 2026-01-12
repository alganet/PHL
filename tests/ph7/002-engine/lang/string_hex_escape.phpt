--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String with hex escape sequences
--FILE--
<?php
echo "\x41\x42\x43", "\n";
echo "\x48\x65\x6c\x6c\x6f", "\n";
?>
--EXPECT--
ABC
Hello
