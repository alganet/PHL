--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid array syntax
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// This should trigger compilation error for invalid array syntax
$a = array(1, 2, 3 => );
echo "Should not reach here";
?>
--EXPECTF--
%s Fatal error:  array(): Missing entry value %s
--CLEAN--
<?php
unset($a);
