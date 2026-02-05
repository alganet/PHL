--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func simple function
--FILE--
<?php
function cufdouble($x){ return $x * 2; }
echo call_user_func('cufdouble', 4) . "\n";
?>
--EXPECT--
8
--CLEAN--
<?php

