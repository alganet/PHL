--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: intdiv(9, 3) returns 3
--FILE--
<?php
echo "intdiv=" . intdiv(9, 3) . "\n";
?>
--EXPECT--
intdiv=3
--CLEAN--
<?php

