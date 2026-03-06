--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_keys with non-scalar strict parameter triggers TypeError
--FILE--
<?php
array_keys(array(1, 2), 1, array());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_keys(): Argument #3 ($strict) must be of type bool, %s given in %s
--CLEAN--
<?php

