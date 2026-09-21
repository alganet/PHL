--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A non-static closure in a global constant expression is a compile-time fatal ("Closures in constant expressions must be static")
--FILE--
<?php
const X = function () { return 1; };
echo "unreachable\n";
?>
--EXPECTF--
%AClosures in constant expressions must be static%A
--CLEAN--
<?php
