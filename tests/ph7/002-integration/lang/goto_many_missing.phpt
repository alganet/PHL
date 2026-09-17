--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Goto with many missing labels triggers error limit
--SKIPIF--
<?php
// php ABORTS at the first compile error; PHL keeps compiling and reports every one
// it finds (then "Error count limit reached" past 15). That is a deliberate engine
// difference, not a fidelity gap -- reporting the whole batch is more useful for an
// embedded engine -- so the two can never agree on this output. The FIRST error's
// text is what has to match php, and that is asserted by the single-error tests in
// this directory; this test exists to pin PHL's continuation behavior.
if (function_exists('zend_version')) { echo 'skip php aborts at the first compile error; PHL reports all (engine design)'; }
?>
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
%AFatal error:%A'goto' to undefined label 'label1'%AFatal error:%A'goto' to undefined label 'label2'%AFatal error:%A'goto' to undefined label 'label3'%AFatal error:%A'goto' to undefined label 'label4'%AFatal error:%A'goto' to undefined label 'label5'%AFatal error:%A'goto' to undefined label 'label6'%AFatal error:%A'goto' to undefined label 'label7'%AFatal error:%A'goto' to undefined label 'label8'%AFatal error:%A'goto' to undefined label 'label9'%AFatal error:%A'goto' to undefined label 'label10'%AFatal error:%A'goto' to undefined label 'label11'%AFatal error:%A'goto' to undefined label 'label12'%AFatal error:%A'goto' to undefined label 'label13'%AFatal error:%A'goto' to undefined label 'label14'%AFatal error:%A'goto' to undefined label 'label15'%AFatal error:%AError count limit reached,PH7 is aborting compilation%A
--CLEAN--
<?php

