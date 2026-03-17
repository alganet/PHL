--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_udiff with no callback should throw ArgumentCountError when only one argument is present
--FILE--
<?php
array_udiff(array(1));
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_udiff() expects at least 2 arguments, %d given in %s
--CLEAN--
<?php

