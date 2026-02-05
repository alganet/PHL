--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: echo outputs string
--FILE--
<?php
echo "Hello, World!\n";
?>
--EXPECT--
Hello, World!
--CLEAN--
<?php

