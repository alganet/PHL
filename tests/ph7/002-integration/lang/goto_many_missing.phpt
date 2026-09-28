--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: goto to an undefined label stops at the first one
--FILE--
<?php
goto label1;
goto label2;
goto label3;
goto label4;
goto label5;
goto label6;
goto label7;
goto label8;
goto label9;
goto label10;
goto label11;
goto label12;
goto label13;
goto label14;
goto label15;
goto label16;
?>
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'label1'%A
--CLEAN--
<?php

