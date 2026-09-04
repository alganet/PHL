--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: ASSERT_QUIET_EVAL constant
--SKIPIF--
<?php
// PHL extension: `ASSERT_QUIET_EVAL` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: ASSERT_QUIET_EVAL is not a php symbol'; }
?>
--FILE--
<?php
echo "ASSERT_QUIET_EVAL=" . ASSERT_QUIET_EVAL . "\n";
?>
--EXPECTF--
ASSERT_QUIET_EVAL=%d
--CLEAN--
<?php

