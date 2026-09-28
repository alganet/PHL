--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A qualified type may not end in a reserved class name
--DESCRIPTION--
Reserved words being legal name segments means a type declaration can now spell one as its
trailing segment, and php screens that: it resolves the name first and then refuses it,
naming the RESOLVED name. `extends` and `new` are not screened -- only type declarations are.
--FILE--
<?php
namespace TrqnZ;
function trqn(): TrqnA\int { }
?>
--EXPECTF--
%s Fatal error:  Cannot use "TrqnZ%sTrqnA%sint" as a type name as it is reserved%A
--CLEAN--
<?php
