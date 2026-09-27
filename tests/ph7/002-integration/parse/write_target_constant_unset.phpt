--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unset() of a global constant is a parse error
--FILE--
<?php
define("WRITE_TARGET_UNSET_K", 1);
unset(WRITE_TARGET_UNSET_K);
?>
--EXPECTF--
%Asyntax error, unexpected token ")", expecting "->" or "?->" or "["%A
