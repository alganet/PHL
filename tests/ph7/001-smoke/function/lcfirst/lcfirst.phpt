--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: lcfirst lower-case first character
--FILE--
<?php
echo "lcfirst=" . lcfirst('Hello') . "\n";
?>
--EXPECT--
lcfirst=hello
--CLEAN--
<?php

