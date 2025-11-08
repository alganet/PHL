--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Include
--FILE--
<?php
include __DIR__ . '/012-include_me.php';
echo $included_var;
?>
--EXPECT--
included
--CLEAN--
<?php
unset($included_var);
?>