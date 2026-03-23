--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert_options with invalid option throws ValueError
--FILE--
<?php
error_reporting(E_ALL & ~E_DEPRECATED);
assert_options(999);
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: assert_options(): Argument #1 ($option) must be an ASSERT_* constant in %s
--CLEAN--
<?php

