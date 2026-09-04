--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ph7credits returns TRUE and prints credits (printed content is ignored via ob buffering)
--SKIPIF--
<?php
// PHL extension: `ph7credits()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: ph7credits() is not a php symbol'; }
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
