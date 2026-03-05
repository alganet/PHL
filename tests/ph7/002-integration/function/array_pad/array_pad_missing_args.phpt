--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad with no arguments triggers ArgumentCountError
--FILE--
<?php
array_pad();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_pad() expects exactly 3 arguments, 0 given in %s
--CLEAN--
<?php

