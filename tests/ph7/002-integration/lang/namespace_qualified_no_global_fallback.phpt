--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Qualified namespace function names do not exist as global fallback
--FILE--
<?php
// A\strlen does not exist even though global strlen does.
// Qualified names must not resolve to the global short name.
echo function_exists("A\\strlen") ? "exists" : "not exists", "\n";
echo function_exists("strlen") ? "exists" : "not exists", "\n";
echo function_exists("Bogus\\strtoupper") ? "exists" : "not exists", "\n";
echo function_exists("strtoupper") ? "exists" : "not exists", "\n";
?>
--EXPECT--
not exists
exists
not exists
exists
--CLEAN--
<?php
