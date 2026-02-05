--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_callable detects named function
--FILE--
<?php
function is_callable_test_fn(){}
echo (is_callable('is_callable_test_fn') ? "ok\n" : "fail\n");
?>
--EXPECT--
ok
--CLEAN--
<?php

