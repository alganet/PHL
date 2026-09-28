--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
For loop invalid syntax in initialization
--FILE--
<?php
for($i=0$i<10; $i++) {
}
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected variable "$i", expecting ";"%A
--CLEAN--
<?php

