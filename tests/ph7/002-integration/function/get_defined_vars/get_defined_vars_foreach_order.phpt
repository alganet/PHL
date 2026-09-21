--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_vars(): a foreach ($a as $k => $v) binds the VALUE before the KEY (php symbol-table order)
--DESCRIPTION--
php's symbol table lists a foreach value ahead of its key. PHL binds both frame locals on the
first iteration; the value must be created first so get_defined_vars() reports ...,v,k not ...,k,v.
A recorded residual (the get_defined_vars order residuals). Object iteration key/value order
remains a separate open residual.
--FILE--
<?php
function fe() {
    $before = 0;
    foreach (['x' => 1] as $k => $v) {}
    return implode(',', array_keys(get_defined_vars()));
}
echo fe(), "\n";
?>
--EXPECT--
before,v,k
