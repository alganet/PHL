--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chroot() should return FALSE with no arguments
--SKIPIF--
<?php
// PHL extension: `chroot()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: chroot() is not a php symbol'; }
?>
--FILE--
<?php
echo chroot() ? "true\n" : "false\n";
?>
--EXPECT--
false
--CLEAN--
<?php

