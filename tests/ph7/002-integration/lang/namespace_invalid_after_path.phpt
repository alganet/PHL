--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace with invalid token after path
--FILE--
<?php
namespace my\ns 123;
echo "should not reach here\n";
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected %s "123", expecting "{" %s
--CLEAN--
<?php
