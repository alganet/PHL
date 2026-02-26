--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Second argument must be an array
--FILE--
<?php
array_diff_assoc(array(), 'not array');
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_diff_assoc(): Argument #2 must be of type array, string given in %s
--CLEAN--
<?php

