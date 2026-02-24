--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: abs(-3.14) returns 3.14 (float branch)
--FILE--
<?php
echo "abs=" . abs(-3.14) . "\n";
?>
--EXPECT--
abs=3.14
--CLEAN--
<?php

