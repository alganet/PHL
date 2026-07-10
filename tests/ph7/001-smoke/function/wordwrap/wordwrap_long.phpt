--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
wordwrap wraps at word boundaries and honours the cut flag
--FILE--
<?php
// A single word longer than the width is left intact when cut is false (default).
var_dump(wordwrap("supercalifragilisticexpialidocious", 10));
// With cut enabled the long word is hard-broken at the width.
var_dump(wordwrap("supercalifragilisticexpialidocious", 10, "\n", true));
// Normal word wrapping breaks at spaces.
var_dump(wordwrap("The quick brown fox", 10));
// A word longer than the width overflows onto its own line (cut false).
var_dump(wordwrap("A very longlonglongword here", 10));
// Custom multi-character break.
var_dump(wordwrap("aaa bbb ccc", 3, "<br>\n"));
?>
--EXPECT--
string(34) "supercalifragilisticexpialidocious"
string(37) "supercalif
ragilistic
expialidoc
ious"
string(19) "The quick
brown fox"
string(28) "A very
longlonglongword
here"
string(19) "aaa<br>
bbb<br>
ccc"
--CLEAN--
<?php
