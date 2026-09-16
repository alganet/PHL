--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
echo in expression position is a parse error (was a bare skip asserting the Symisc `$x or echo "..."` extension)
--FILE--
<?php
$x = false;
$x or echo "false or executed\n";
echo "Done\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "echo"%A
--CLEAN--
<?php
