--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 array argument should raise TypeError
--FILE--
<?php
atan2(array(1), 1);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: atan2(): Argument #1 ($y) must be of type float, array given in %s
--CLEAN--
<?php

