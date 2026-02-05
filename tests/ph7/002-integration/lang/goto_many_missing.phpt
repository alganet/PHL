--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Goto with many missing labels triggers error limit
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
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
%s 2 Error: Label 'label1' was referenced but not defined
%s 3 Error: Label 'label2' was referenced but not defined
%s 4 Error: Label 'label3' was referenced but not defined
%s 5 Error: Label 'label4' was referenced but not defined
%s 6 Error: Label 'label5' was referenced but not defined
%s 7 Error: Label 'label6' was referenced but not defined
%s 8 Error: Label 'label7' was referenced but not defined
%s 9 Error: Label 'label8' was referenced but not defined
%s 10 Error: Label 'label9' was referenced but not defined
%s 11 Error: Label 'label10' was referenced but not defined
%s 12 Error: Label 'label11' was referenced but not defined
%s 13 Error: Label 'label12' was referenced but not defined
%s 14 Error: Label 'label13' was referenced but not defined
%s 15 Error: Label 'label14' was referenced but not defined
%s 16 Error: Label 'label15' was referenced but not defined
%s 17 Error count limit reached,PH7 is aborting compilation
Compile error
--CLEAN--
<?php

