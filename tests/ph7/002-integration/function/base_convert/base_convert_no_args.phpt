--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert no arguments throws ArgumentCountError
--FILE--
<?php
base_convert();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: base_convert() expects exactly 3 arguments, 0 given in %s
--CLEAN--
<?php

