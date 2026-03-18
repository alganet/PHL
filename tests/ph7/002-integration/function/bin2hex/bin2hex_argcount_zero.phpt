--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling bin2hex() without arguments should raise an ArgumentCountError
--FILE--
<?php
bin2hex();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: bin2hex() expects exactly 1 argument, 0 given in %s
--CLEAN--
<?php

