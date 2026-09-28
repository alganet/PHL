--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An immediately invoked closure in a constant expression is a CALL, and takes the call sentence
--FILE--
<?php
class CesD {
    const C = (function () { return 1; })();
}
echo "unreachable\n";
?>
--EXPECTF--
%AConstant expression contains invalid operations%A
--CLEAN--
<?php
