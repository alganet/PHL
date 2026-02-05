--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
unexpected closing curly brace
--FILE--
<?php
if (true) {
    echo 1;
}
echo 2;
}
?>
--EXPECTF--
%s 6 Error: Syntax error: Unexpected token '}'
Compile error
--CLEAN--
<?php

