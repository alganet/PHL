--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect with no callback should throw ArgumentCountError when only one argument is present
--FILE--
<?php
array_uintersect(array(1));
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_uintersect() expects at least 2 arguments, %d given in %s
--CLEAN--
<?php

