--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$1 names the integer and says it expected a variable, "{" or "$" (was a bare skip: it used to drift into a modifiable-l-value complaint)

--FILE--
<?php
// Test invalid variable name to cover line 1440 in compile.c
$1 = 5;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected integer "1", expecting variable or "{" or "$"%A
--CLEAN--
<?php
unset($1);
