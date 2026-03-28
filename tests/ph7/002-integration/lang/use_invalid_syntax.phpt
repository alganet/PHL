--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Use statement with invalid syntax
--FILE--
<?php
use 123;
echo "should not reach here\n";
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected %s "123" %s
--CLEAN--
<?php
