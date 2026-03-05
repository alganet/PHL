--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_search with no arguments triggers ArgumentCountError
--FILE--
<?php
array_search();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_search() expects at least 2 arguments, %d given in %s
--CLEAN--
<?php

