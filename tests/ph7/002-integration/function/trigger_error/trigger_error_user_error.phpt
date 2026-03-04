--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
trigger_error with E_USER_ERROR aborts execution
--SKIPIF--
<?php
if (!function_exists('trigger_error')) { echo 'skip: trigger_error not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP E_USER_ERROR behavior differs'; }
?>
--FILE--
<?php
echo "before_error\n";
trigger_error("Fatal error", E_USER_ERROR);
echo "after_error\n";
?>
--EXPECTF--
before_error
%s Error:  Fatal error
--CLEAN--
<?php

