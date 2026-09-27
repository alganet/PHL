--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array entry taken by reference is a full write target
--FILE--
<?php
// `[&f()]` is php's compile fatal, not the `$r =& f()` exemption.
function writeTargetArrayRefCall() { return [1,2]; }
$a = [&writeTargetArrayRefCall()];
?>
--EXPECTF--
%ACan't use function return value in write context%A
