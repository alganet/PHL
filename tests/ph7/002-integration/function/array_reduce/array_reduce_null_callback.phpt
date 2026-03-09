--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce with null callback throws TypeError
--FILE--
<?php
array_reduce(array(1, 2), null);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_reduce(): Argument #2 ($callback) must be a valid callback, no array or string given in %s
--CLEAN--
<?php

