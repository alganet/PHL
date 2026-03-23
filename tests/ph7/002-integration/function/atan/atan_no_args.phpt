--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan missing argument should throw ArgumentCountError
--FILE--
<?php
atan();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: atan() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

