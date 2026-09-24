--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: include_once through a php://filter URL runs the body and is marked
--DESCRIPTION--
include_path names DIRECTORIES, so it has nothing to say about a URL — and the
arm that skipped it was also the arm that marks a file as INCLUDED, so
include_once through a php://filter URL answered success without running the
body at all, and a plain include left the file stack unbalanced.
--FILE--
<?php
$sfoFile = tempnam(sys_get_temp_dir(), 'sfo');
file_put_contents($sfoFile, '<?php echo "BODY RAN\n";');
$sfoUrl = "php://filter/read=string.tolower/resource=$sfoFile";
var_dump((bool) (include_once $sfoUrl));
var_dump((bool) (include_once $sfoUrl));
include $sfoUrl;
@unlink($sfoFile);
?>
--EXPECT--
body ran
bool(true)
bool(true)
body ran
