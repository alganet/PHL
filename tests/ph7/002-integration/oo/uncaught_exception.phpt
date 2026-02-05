--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Uncaught exception reported as fatal
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
throw new Exception("uncaught");
?>
--EXPECTF--
%s Error: Uncaught exception 'Exception' in the 'Global' frame context
--CLEAN--
<?php

