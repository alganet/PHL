--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ends_with emits E_DEPRECATED on null and falls through to coerce as ""
--FILE--
<?php
set_error_handler(function ($errno, $errstr) {
    echo "DEP[" . $errno . "]: " . $errstr . "\n";
    return true;
});
echo "h_null=" . (str_ends_with(null, "x") ? 'true' : 'false') . "\n";
echo "n_null=" . (str_ends_with("abc", null) ? 'true' : 'false') . "\n";
?>
--EXPECT--
DEP[8192]: str_ends_with(): Passing null to parameter #1 ($haystack) of type string is deprecated
h_null=false
DEP[8192]: str_ends_with(): Passing null to parameter #2 ($needle) of type string is deprecated
n_null=true
--CLEAN--
<?php

