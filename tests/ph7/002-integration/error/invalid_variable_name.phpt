--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$( names the "(" and says it expected a variable, "{" or "$" (was a bare skip: the bad token used to drift into a later node and surface as an error on the ";")

--FILE--
<?php
$result = $(;
echo "Result: $result\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "(", expecting variable or "{" or "$"%A
--CLEAN--
<?php
unset($result);
