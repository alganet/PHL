--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
return inside a finally reached by an exception from a nested call returns that value from the enclosing function
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
function inner_throw() {
    throw new RuntimeException("boom");
}
function f_ret() {
    try {
        inner_throw();
    } finally {
        return "fin-ret";
    }
}
echo "A:", f_ret(), "\n";
echo "end\n";
--EXPECT--
A:fin-ret
end
--CLEAN--
<?php
