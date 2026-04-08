--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: intdiv(0, 5) returns 0
--FILE--
<?php
echo "intdiv=" . intdiv(0, 5) . "\n";
?>
--EXPECT--
intdiv=0
--CLEAN--
<?php

