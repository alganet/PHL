--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
${} names the "}" with no expecting-clause, as php does (was a bare skip freezing PHL's extra clause)

--FILE--
<?php
$a = ${};
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "}"%A
--CLEAN--
<?php
unset($a);
