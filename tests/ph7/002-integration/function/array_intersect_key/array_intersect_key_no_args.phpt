--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_key with no arguments triggers ArgumentCountError
--FILE--
<?php
array_intersect_key();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_intersect_key() expects at least 1 argument, 0 given in %s
--CLEAN--
<?php

