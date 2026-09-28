--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unexpected closing parenthesis
--FILE--
<?php
echo 1);
?>
--EXPECTF--
%AParse error:%AUnmatched ')'%A
--CLEAN--
<?php

