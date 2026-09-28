--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unexpected end of file is reported where the end of the file sits
--DESCRIPTION--
php reports its `unexpected end of file` at the line the file ENDS on, which is past the last
token whenever anything follows it -- and a trailing newline always does. PHL carried the last
TOKEN's line, so a file ending in blank lines named a line several above the one php names.
A `?>` would close the chunk and leave nothing unfinished, so this one has none.
--FILE--
<?php
$eelw = 1


--EXPECTF--
%s Parse error:  syntax error, unexpected end of file in %s on line 4%A
--CLEAN--
<?php
