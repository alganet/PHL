--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad with too few arguments triggers ArgumentCountError
--FILE--
<?php
array_pad(array(1, 2), 5);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_pad() expects exactly 3 arguments, 2 given in %s
--CLEAN--
<?php

