--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strtr with from-to strings
--FILE--
<?php
echo strtr("hello", "ho", "HO") . "\n";
?>
--EXPECT--
HellO
--CLEAN--
<?php

