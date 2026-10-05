--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The "producing output" deprecation is displayed one level down, and at shutdown under no function
--DESCRIPTION--
php raises it while the handler's buffer is disabled, so its display copy bypasses that
buffer: a single level writes it straight out ahead of the handler's answer, a nested
level writes it into the buffer below -- which, the handler still counting as running,
marks THAT buffer, so its own handler is deprecated next even though it never printed.
The shutdown flush names "PHP Request Shutdown" with no "()" and no location.
--INI--
error_reporting=-1
display_errors=1
log_errors=0
html_errors=0
--FILE--
<?php
function h($b) { echo "X"; return "[$b]"; }
function silent($b) { return "{" . $b . "}"; }
echo "1:"; ob_start('h'); echo "a"; ob_end_flush(); echo "\n";
echo "2:"; ob_start('silent'); ob_start('h'); echo "b"; ob_end_flush(); ob_end_flush(); echo "\n";
echo "3:"; ob_start('h'); ob_start('h'); echo "b"; ob_end_flush(); ob_end_flush(); echo "\n";
echo "4:"; ob_start('h'); echo "end";
--EXPECTF--
1:
Deprecated: ob_end_flush(): Producing output from user output handler h is deprecated in %s on line 4
[a]
2:
Deprecated: ob_end_flush(): Producing output from user output handler silent is deprecated in %s on line 5
{
Deprecated: ob_end_flush(): Producing output from user output handler h is deprecated in %s on line 5
[b]}
3:
Deprecated: ob_end_flush(): Producing output from user output handler h is deprecated in %s on line 6
[
Deprecated: ob_end_flush(): Producing output from user output handler h is deprecated in %s on line 6
[b]]
4:
Deprecated: PHP Request Shutdown: Producing output from user output handler h is deprecated in Unknown on line 0
[end]