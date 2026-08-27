--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_object function
--FILE--
<?php
var_dump(is_object(null));
?>
--EXPECT--
bool(false)
--CLEAN--
<?php

