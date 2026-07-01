--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An explicit return in a never-returning function is a compile error
--FILE--
<?php
function bad(): never { return; }
?>
--EXPECTF--
%s Fatal error:  A never-returning function must not return in %s
--CLEAN--
<?php
