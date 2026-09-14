--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
trigger_error(E_USER_ERROR) is a ValueError (php deprecates; PHL removes the level)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL removes what php only deprecates'; ?>
--FILE--
<?php
// php only DEPRECATES passing E_USER_ERROR to trigger_error() (then still fatals);
// PHL targets php's non-deprecated surface and rejects the level with a ValueError.
echo "before\n";
try {
    trigger_error("boom", E_USER_ERROR);
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
// the non-deprecated levels still work
trigger_error("a notice", E_USER_NOTICE);
echo "after\n";
?>
--EXPECTF--
before
trigger_error(): Passing E_USER_ERROR is no longer supported, throw an exception or call exit() with a string message instead
%Aa notice%Aafter
