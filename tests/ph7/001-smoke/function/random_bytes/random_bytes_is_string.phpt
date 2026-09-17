--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_bytes returns a string

--FILE--
<?php
echo is_string(random_bytes(8)) ? "str_ok\n" : "str_fail\n";
?>
--EXPECT--
str_ok
--CLEAN--
<?php
