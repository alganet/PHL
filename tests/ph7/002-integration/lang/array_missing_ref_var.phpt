--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array(&) is a plain syntax error on the ')' (was a bare skip freezing PHL's invented "array(): Missing referenced variable" fatal)

--FILE--
<?php
$x = array(&);
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ")"%A
--CLEAN--
<?php
unset($x);
