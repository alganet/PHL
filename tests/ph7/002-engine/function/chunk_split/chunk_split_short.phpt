--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chunk_split with short string (chunk length > string length)
--FILE--
<?php
echo chunk_split('abc', 10, ':') . "\n";
?>
--EXPECT--
abc: