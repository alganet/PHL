--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert two arguments throws ArgumentCountError
--FILE--
<?php
base_convert("10", 10);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: base_convert() expects exactly 3 arguments, 2 given in %s
--CLEAN--
<?php

