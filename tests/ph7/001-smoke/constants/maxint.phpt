--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: MAXINT maps to PHP_INT_MAX
--SKIPIF--
<?php
// PHL extension: `MAXINT` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: MAXINT is not a php symbol'; }
?>
--FILE--
<?php
// MAXINT mirrors PHP_INT_MAX
echo "MAXINT=" . MAXINT . "\n";
?>
--EXPECTF--
MAXINT=%d
--CLEAN--
<?php
// nothing to clean

