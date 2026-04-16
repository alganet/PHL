--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
In weak mode, returning a strictly-numeric string for :int coerces
--FILE--
<?php
function st_weak_ret_numstr(): int { return "42"; }
echo st_weak_ret_numstr(), "\n";
?>
--EXPECT--
42
