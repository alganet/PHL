--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self, parent and static are not class names a qualifier may reach
--FILE--
<?php
namespace TrrcwZ;
function trrcw(): \self { }
?>
--EXPECTF--
%s Fatal error:  '%sself' is an invalid class name%A
--CLEAN--
<?php
