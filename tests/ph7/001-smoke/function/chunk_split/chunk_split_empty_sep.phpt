--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chunk_split with empty separator
--FILE--
<?php
echo chunk_split("hello",76,"") . "\n";
?>
--EXPECT--
hello
--CLEAN--
<?php

