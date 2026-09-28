--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter default may not hold a non-static closure or an arrow function either
--FILE--
<?php
function cesParamClo($x = function () {}) { return 1; }
echo "unreachable\n";
?>
--EXPECTF--
%AClosures in constant expressions must be static%A
--CLEAN--
<?php
