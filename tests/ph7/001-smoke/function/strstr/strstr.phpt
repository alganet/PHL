--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strstr returns substring from first matched occurrence
--FILE--
<?php
echo strstr("hello world","lo") . "\n"; // expecting 'lo world'
?>
--EXPECT--
lo world
--CLEAN--
<?php

