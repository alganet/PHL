--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asin missing argument should throw ArgumentCountError
--FILE--
<?php
asin();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: asin() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

