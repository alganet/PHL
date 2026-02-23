--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
acos array argument should raise TypeError
--FILE--
<?php
acos(array(1));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: acos(): Argument #1 ($num) must be of type float, array given in %s
--CLEAN--
<?php

?>