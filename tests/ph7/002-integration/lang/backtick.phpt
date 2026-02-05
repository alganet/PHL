--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
backtick quoted string
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo `echo test`;
?>
--EXPECTF--
%s 2 Notice: Command line invocation is disabled in the current release of the PH7(2.1.4) engine
--CLEAN--
<?php

