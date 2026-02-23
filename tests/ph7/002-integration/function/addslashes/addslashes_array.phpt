--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes(array()) should raise a TypeError like PHP
--FILE--
<?php
addslashes(array());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: addslashes(): Argument #1 ($string) must be of type string, array given in %s

--CLEAN--
<?php

