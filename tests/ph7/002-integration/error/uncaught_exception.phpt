--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
uncaught exception error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
throw new Exception("test exception");
?>
--EXPECTF--
%s Error: Uncaught exception 'Exception' in the 'Global' frame context
--CLEAN--
<?php

