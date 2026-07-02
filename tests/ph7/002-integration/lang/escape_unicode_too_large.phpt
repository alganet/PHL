--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
\u{...} beyond U+10FFFF is a compile-time "Codepoint too large" error
--FILE--
<?php
echo "never printed: the whole file fails to compile\n";
echo "\u{110000}";
?>
--EXPECTF--
%Aerror:%AInvalid UTF-8 codepoint escape sequence: Codepoint too large in %s on line %d%A
--CLEAN--
<?php
