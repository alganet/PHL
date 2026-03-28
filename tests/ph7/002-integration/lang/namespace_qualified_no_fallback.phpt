--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unqualified function calls in namespace fall back to global
--FILE--
<?php
namespace App;

// Unqualified calls should fall back to global built-in functions
echo strlen("test"), "\n";
echo strtoupper("hello"), "\n";
echo abs(-5), "\n";
?>
--EXPECT--
4
HELLO
5
--CLEAN--
<?php
