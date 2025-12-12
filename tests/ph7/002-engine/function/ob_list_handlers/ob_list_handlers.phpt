--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_list_handlers returns non-empty array when buffer is active
--SKIPIF--
<?php
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
?>
--FILE--
<?php
// Ensure at least one handler is present
ob_start();
$l = ob_list_handlers();
ob_end_clean();
echo is_array($l) && count($l) > 0 ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
