--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
\o octal escape with 0 in double quoted string
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo "\"" . "\o0" . "\"";
?>
--EXPECT--
""
--CLEAN--
<?php

