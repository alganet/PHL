--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
settype() vivifies an undefined variable through its by-ref parameter
--FILE--
<?php
settype($u, "integer");
var_dump($u);
?>
--EXPECT--
int(0)
