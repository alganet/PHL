--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto undefined label
--FILE--
<?php
goto LABEL;
?>
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'LABEL'%A
--CLEAN--
<?php

