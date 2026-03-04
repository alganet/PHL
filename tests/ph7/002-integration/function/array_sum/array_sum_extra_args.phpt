--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_sum with too many arguments triggers ArgumentCountError
--FILE--
<?php
array_sum(array(1), array(2));
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_sum() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

