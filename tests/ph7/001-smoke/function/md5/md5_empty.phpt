--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
md5 of the empty string is the well-defined empty digest (not "")
--FILE--
<?php
echo md5(""), "\n";
echo bin2hex(md5("", true)), "\n";
?>
--EXPECT--
d41d8cd98f00b204e9800998ecf8427e
d41d8cd98f00b204e9800998ecf8427e
--CLEAN--
<?php
