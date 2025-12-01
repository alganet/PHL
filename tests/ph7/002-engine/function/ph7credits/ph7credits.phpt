--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ph7credits returns TRUE and prints credits (printed content is ignored via ob buffering)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
ob_start();
ph7credits();
$s = ob_get_clean();
// Ensure we got something resembling the PH7 credits HTML: look for the "PH7"
echo (strpos($s, 'PH7') !== false) ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($s);
?>
