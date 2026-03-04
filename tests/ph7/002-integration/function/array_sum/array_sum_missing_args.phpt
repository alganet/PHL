--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_sum with no arguments triggers ArgumentCountError
--FILE--
<?php
array_sum();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_sum() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

