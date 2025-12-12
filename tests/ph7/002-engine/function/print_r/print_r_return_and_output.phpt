--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
print_r return and output behavior
--FILE--
<?php
// Capture output via return parameter
$a = array('x'=>1,'y'=>2);
$s = print_r($a, true);
echo is_string($s) ? 'ret_ok' : 'ret_err';
echo "\n";

// Print output directly
ob_start();
print_r($a);
$out = ob_get_clean();
echo (strpos($out,'x') !== false ? 'out_ok' : 'out_err') . "\n";
?>
--EXPECT--
ret_ok
out_ok
