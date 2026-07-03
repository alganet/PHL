--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
KNOWN DIVERGENCE (BYTECODE.md stage 2): a cross-frame finally runs detached from its owning frame, so its `return` value is lost (empty). Pinned until the activation-record rework; the _zend twin is the acceptance test.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
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
A:
end
--CLEAN--
<?php
