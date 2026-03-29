--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
const without equals (covers compile.c lines 492,494)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
const MYCONST;
?>
--EXPECTF--
%s Fatal error:  const: Expected '=' after constant name %s
--CLEAN--
<?php

