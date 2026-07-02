--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Malformed \u{...} escape is a compile-time error (php: Parse error, phl: Fatal error)
--FILE--
<?php
echo "never printed: the whole file fails to compile\n";
echo "\u{}";
?>
--EXPECTF--
%Aerror:%AInvalid UTF-8 codepoint escape sequence in %s on line %d%A
--CLEAN--
<?php
