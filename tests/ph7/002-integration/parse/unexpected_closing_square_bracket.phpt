--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unexpected closing square bracket
--FILE--
<?php
echo $a];
?>
--EXPECTF--
%AParse error:%AUnmatched ']'%A
--CLEAN--
<?php
unset($a);
