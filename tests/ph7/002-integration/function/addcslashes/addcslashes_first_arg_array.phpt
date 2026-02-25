--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes(array(), 'a') should raise TypeError for first parameter
--FILE--
<?php
addcslashes(array(), 'a');
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: addcslashes(): Argument #1 ($string) must be of type string, %s given in %s
--CLEAN--
<?php


