--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
list() without an assignment names the ";" and expects "=" (was a bare skip: PHL named the "list" the construct started at)

--FILE--
<?php
// This should trigger a compile error: "list(): expecting '=' after construct"
list();
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";", expecting "="%A
--CLEAN--
<?php

