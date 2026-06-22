--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sha1 of the empty string is the well-defined empty digest (not "")
--FILE--
<?php
echo sha1(""), "\n";
echo bin2hex(sha1("", true)), "\n";
?>
--EXPECT--
da39a3ee5e6b4b0d3255bfef95601890afd80709
da39a3ee5e6b4b0d3255bfef95601890afd80709
--CLEAN--
<?php
