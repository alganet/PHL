--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: parent constant returns null when used outside class context
--SKIPIF--
<?php
// PHL extension: `parent` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: parent is not a php symbol'; }
?>
--FILE--
<?php
if (parent === null) {
    echo "NULL\n";
} else {
    echo "not null\n";
}
?>
--EXPECT--
NULL
--CLEAN--
<?php

