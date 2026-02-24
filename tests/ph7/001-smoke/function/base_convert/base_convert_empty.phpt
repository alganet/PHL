--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with empty string
--SKIPIF--
<?php 
if (function_exists('zend_version')) echo 'skip';
--FILE--
<?php
echo base_convert("", 10, 16) . "\n";
?>
--EXPECT--
--CLEAN--
<?php

