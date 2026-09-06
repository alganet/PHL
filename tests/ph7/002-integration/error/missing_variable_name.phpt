--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Missing variable name in dynamic variable syntax
--FILE--
<?php
$result = ${;};
echo "Result: $result\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";"%A
--CLEAN--
<?php
unset($result);
