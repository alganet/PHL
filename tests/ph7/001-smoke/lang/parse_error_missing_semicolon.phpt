--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Parse error missing semicolon

--FILE--
<?php
echo "hello"
?>
--EXPECT--
hello
--CLEAN--
<?php

