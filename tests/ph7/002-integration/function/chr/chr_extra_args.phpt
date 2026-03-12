--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chr() with too many arguments should emit ArgumentCountError
--FILE--
<?php
chr(65, 66);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: chr() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

