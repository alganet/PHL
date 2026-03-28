--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Elvis operator ?: basic usage
--FILE--
<?php
echo "hello" ?: "world", "\n";
echo "" ?: "fallback", "\n";
echo 0 ?: 42, "\n";
echo null ?: "default", "\n";
echo false ?: "yes", "\n";
echo true ?: "no", "\n";
echo 5 ?: 10, "\n";
?>
--EXPECT--
hello
fallback
42
default
yes
1
5
--CLEAN--
<?php
