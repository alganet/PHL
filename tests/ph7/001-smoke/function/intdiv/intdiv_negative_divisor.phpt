--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: intdiv(7, -2) returns -3
--FILE--
<?php
echo "intdiv=" . intdiv(7, -2) . "\n";
?>
--EXPECT--
intdiv=-3
--CLEAN--
<?php

