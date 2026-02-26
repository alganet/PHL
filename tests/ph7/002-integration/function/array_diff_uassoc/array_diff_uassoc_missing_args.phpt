--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_uassoc with no arguments triggers ArgumentCountError
--FILE--
<?php
array_diff_uassoc();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_diff_uassoc() expects at least 2 arguments, %d given in %s
--CLEAN--
<?php

