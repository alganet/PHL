--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes writes php's \a and \b for BEL and BACKSPACE
--FILE--
<?php
echo addcslashes("\x07", "\0..\37"), "\n";
echo addcslashes("\x08", "\0..\37"), "\n";
echo bin2hex(addcslashes("\x07\x08", "\0..\37")), "\n";
?>
--EXPECT--
\a
\b
5c615c62
--CLEAN--
<?php
