--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A non-static closure in a class constant initializer is a compile-time fatal ("Closures in constant expressions must be static")
--FILE--
<?php
class C {
    const K = function () { return 1; };
}
echo "unreachable\n";
?>
--EXPECTF--
%AClosures in constant expressions must be static%A
--CLEAN--
<?php
