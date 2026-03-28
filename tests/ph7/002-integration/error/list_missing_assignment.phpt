--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
list() construct missing assignment operator error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// This should trigger a compile error: "list(): expecting '=' after construct"
list();
?>
--EXPECTF--
%s Error:  list(): expecting '=' after construct %s
--CLEAN--
<?php

