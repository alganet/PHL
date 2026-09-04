--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: UTF8 constant
--SKIPIF--
<?php
// PHL extension: `UTF8` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: UTF8 is not a php symbol'; }
?>
--FILE--
<?php
echo "UTF8=" . UTF8 . "\n";
?>
--EXPECTF--
UTF8=%s
--CLEAN--
<?php

