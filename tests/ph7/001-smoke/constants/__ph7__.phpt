--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: __PH7__ string value
--SKIPIF--
<?php
// PHL extension: `__PH7__` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: __PH7__ is not a php symbol'; }
?>
--FILE--
<?php
echo "__PH7__=" . __PH7__ . "\n";
?>
--EXPECTF--
__PH7__=PH7/%s
--CLEAN--
<?php

