--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quoted_printable_decode stops at the first NUL byte
--FILE--
<?php
echo bin2hex(quoted_printable_decode("a\x00b")), "\n";
echo bin2hex(quoted_printable_decode("=41\x00=42")), "\n";
echo bin2hex(quoted_printable_encode("a\x00b")), "\n";
?>
--EXPECT--
61
41
613d303062
--CLEAN--
<?php
