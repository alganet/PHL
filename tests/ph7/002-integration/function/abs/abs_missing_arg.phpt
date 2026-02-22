--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
abs() with no arguments should emit PHP-compatible uncaught ArgumentCountError message
--FILE--
<?php
abs();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: abs() expects exactly 1 argument, %d given in %s

--CLEAN--
<?php

?>
