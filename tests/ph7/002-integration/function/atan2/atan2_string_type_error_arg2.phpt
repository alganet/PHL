--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 non-numeric string second argument should raise TypeError
--FILE--
<?php
atan2(1, "bar");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: atan2(): Argument #2 ($x) must be of type float, string given in %s
--CLEAN--
<?php

