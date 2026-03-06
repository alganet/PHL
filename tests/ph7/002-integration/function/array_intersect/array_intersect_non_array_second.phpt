--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect with non-array second argument triggers TypeError
--FILE--
<?php
array_intersect(array(1, 2, 3), 'not_array');
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_intersect(): Argument #2 must be of type array, string given in %s
--CLEAN--
<?php

