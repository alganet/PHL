--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declare with encoding directive
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
declare(encoding='UTF-8');
echo "OK";
?>
--EXPECTF--
%s 2 Notice: the declare construct is a no-op in the current release of the PH7(2.1.4) engine
OK
