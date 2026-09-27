--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf specializes only when its format arithmetic works out
--FILE--
<?php
// php needs a literal format under 256 bytes carrying nothing but %s, %d and
// %%, with exactly one value per placeholder. One placeholder, one value: the
// opcode, and the refusal.
sprintf("%s", "b")[0] = 1;
?>
--EXPECTF--
%ACannot use result of built-in function in write context%A
