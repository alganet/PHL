--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: PATHINFO_FILENAME holds php's value
--FILE--
<?php
echo PATHINFO_FILENAME . "\n";
?>
--EXPECT--
8
--CLEAN--
<?php

