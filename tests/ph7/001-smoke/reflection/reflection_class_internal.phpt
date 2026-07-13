--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass on internal vs user classes
--FILE--
<?php
class ReflIntUser {}

$re = new ReflectionClass('Exception');
echo $re->isInternal() ? 'internal' : 'user', "\n";
echo $re->isUserDefined() ? 'userdef' : 'not-userdef', "\n";
echo $re->getFileName() === false ? 'no-file' : 'file', "\n";
echo $re->getStartLine() === false ? 'no-start' : 'start', "\n";
echo $re->getEndLine() === false ? 'no-end' : 'end', "\n";
echo $re->getDocComment() === false ? 'no-doc' : 'doc', "\n";

$ru = new ReflectionClass('ReflIntUser');
echo $ru->isInternal() ? 'internal' : 'user', "\n";
echo $ru->isUserDefined() ? 'userdef' : 'not-userdef', "\n";
echo $ru->getFileName() === false ? 'no-file' : 'file', "\n";
echo is_int($ru->getStartLine()) ? 'start-int' : 'no-start', "\n";
echo $ru->getDocComment() === false ? 'no-doc' : 'doc', "\n";
?>
--EXPECT--
internal
not-userdef
no-file
no-start
no-end
no-doc
user
userdef
file
start-int
no-doc
