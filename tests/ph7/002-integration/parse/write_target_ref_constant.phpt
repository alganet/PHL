--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A reference source has to be a variable too
--FILE--
<?php
define("WRITE_TARGET_REF_K", 1);
$r =& WRITE_TARGET_REF_K;
?>
--EXPECTF--
%Asyntax error, unexpected token ";", expecting "->" or "?->" or "["%A
