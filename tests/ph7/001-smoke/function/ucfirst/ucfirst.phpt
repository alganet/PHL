--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ucfirst upper-case first character
--FILE--
<?php
echo "ucfirst=" . ucfirst('hello') . "\n";
?>
--EXPECT--
ucfirst=Hello
--CLEAN--
<?php

