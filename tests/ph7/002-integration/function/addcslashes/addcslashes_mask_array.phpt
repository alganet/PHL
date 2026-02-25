--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes('a', array()) should raise TypeError for second parameter
--FILE--
<?php
addcslashes('a', array());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: addcslashes(): Argument #2 ($characters) must be of type string, %s given in %s
--CLEAN--
<?php


