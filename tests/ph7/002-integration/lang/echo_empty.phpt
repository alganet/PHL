--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
echo ; is a parse error
--FILE--
<?php
echo ;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token ";"%A
--CLEAN--
<?php
