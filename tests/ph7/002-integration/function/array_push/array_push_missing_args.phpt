--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_push with no arguments triggers ArgumentCountError
--FILE--
<?php
array_push();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_push() expects at least 1 argument, %d given in %s
--CLEAN--
<?php

