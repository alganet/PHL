--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PATHINFO_EXTENSION holds php's value
--FILE--
<?php
echo PATHINFO_EXTENSION . "\n";
?>
--EXPECT--
4
--CLEAN--
<?php

