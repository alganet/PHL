--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unexpected closing curly brace
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
if (true) {
    echo 1;
}
echo 2;
}
?>
--EXPECTF--
%s Fatal error:  Syntax error: Unexpected token '}' %s
--CLEAN--
<?php

