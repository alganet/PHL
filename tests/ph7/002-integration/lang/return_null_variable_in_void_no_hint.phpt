--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the void hint is the null KEYWORD, not a null value: a variable gets no hint
--FILE--
<?php
function bad(): void {
    $n = null;
    return $n;
}
?>
--EXPECTF--
%s Fatal error:  A void function must not return a value in %s
--CLEAN--
<?php
