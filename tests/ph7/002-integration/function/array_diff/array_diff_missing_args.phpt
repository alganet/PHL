--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff with no arguments triggers ArgumentCountError
--FILE--
<?php
array_diff();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_diff() expects at least 1 argument, %d given in %s
--CLEAN--
<?php

