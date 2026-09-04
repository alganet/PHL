--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DIR_SEP value
--SKIPIF--
<?php
// PHL extension: `DIR_SEP` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: DIR_SEP is not a php symbol'; }
?>
--FILE--
<?php
if (PHP_OS == 'WINNT') {
    if (DIR_SEP == '\\') {
        echo "ok";
    } else {
        echo "not ok: " . DIR_SEP . " " . PHP_OS;
    }
} else {
    if (DIR_SEP == '/') {
        echo "ok";
    } else {
        echo "not ok: " . DIR_SEP . " " . PHP_OS;
    }
}
?>
--EXPECT--
ok
--CLEAN--
<?php

