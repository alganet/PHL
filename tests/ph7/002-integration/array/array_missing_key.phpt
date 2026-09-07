--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array entry with no key before '=>' is a parse error
--DESCRIPTION--
Lives in 002-integration, not 001-smoke: a PARSE error produces no program output, and in
the shared in-process smoke interpreter it bails the whole run. A child process shows it.
--FILE--
<?php
$a = array( => 2);
echo "FAIL: compiled";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "=>", expecting ")"%A
--CLEAN--
<?php
