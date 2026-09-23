--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The keyword-shaped const names php still accepts keep working
--FILE--
<?php
// php's lexer does not reserve the type words or the scope words, so these parse
// as ordinary constant names — the reserved-word refusal must not reach them.
const int = 1;
const float = 2;
const string = 3;
const bool = 4;
const object = 5;
const self = 6;
const parent = 7;
echo int + float + string + bool + object + self + parent, "\n";
// A class constant may carry a genuinely reserved word; the file-scope one may not.
class ConstNameKw { const list = 8; const class_ = 9; }
echo ConstNameKw::list, "\n";
?>
--EXPECT--
28
8
