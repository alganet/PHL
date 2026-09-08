--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_error_handler and get_exception_handler report the active handler (php 8.5)
--FILE--
<?php
echo get_error_handler() === null ? "null" : "?", "\n";
set_error_handler("strlen");
echo get_error_handler(), "\n";
restore_error_handler();
echo get_error_handler() === null ? "null" : "?", "\n";
set_error_handler(function ($n, $s) { return true; });
echo is_callable(get_error_handler()) ? "callable" : "?", "\n";
echo get_exception_handler() === null ? "null" : "?", "\n";
set_exception_handler(function ($e) {});
echo is_callable(get_exception_handler()) ? "callable" : "?", "\n";
--EXPECT--
null
strlen
null
callable
null
callable
--CLEAN--
<?php
