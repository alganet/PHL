--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto statement with undefined label
--FILE--
<?php
goto MISSING_LABEL;
echo "This should not execute\n";
UNDEFINED_LABEL:
echo "Defined label\n";
?>
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'MISSING_LABEL'%A
--CLEAN--
<?php

