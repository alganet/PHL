--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
header() is silently ignored in CLI mode
--FILE--
<?php
header("X-Foo: bar");
echo count(headers_list());
?>
--EXPECT--
0
