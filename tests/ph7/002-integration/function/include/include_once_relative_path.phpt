--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
include_once/require_once run a bare and a subdir-relative path exactly once
--DESCRIPTION--
Regression: include_once/require_once with a relative path that does NOT start
with ./ or ../ (a bare `file.php` or `subdir/file.php`) went through the
include-path search branch, which pushed the resolved path onto the included-set
AND then pushed the raw relative path a second time. realpath() collapsed both to
the same canonical path, so the second push saw it as "already included" and reset
the isNew out-param to 0 — making the first include silently no-op while returning
truthy. Direct forms (./, ../, absolute) pushed once and worked. Now every
relative form runs on first include and is deduped on subsequent includes, matching
PHP. chdir(__DIR__) first so the bare/relative spellings resolve here regardless of
the harness CWD.
--FILE--
<?php
chdir(__DIR__);
file_put_contents('rel_once_bare.php', "<?php\n\$GLOBALS['once_hits'] = (\$GLOBALS['once_hits'] ?? 0) + 1;\ndefine('REL_ONCE_BARE', 1);\n");
@mkdir('rel_once_sub');
file_put_contents('rel_once_sub/nested.php', "<?php\ndefine('REL_ONCE_NESTED', 1);\n");

$GLOBALS['once_hits'] = 0;

// Bare relative (no leading ./) — the previously-broken form. Body must run.
include_once 'rel_once_bare.php';
echo 'bare defined: ', defined('REL_ONCE_BARE') ? 'yes' : 'no', "\n";

// subdir/-relative require_once — the other broken form. Body must run.
require_once 'rel_once_sub/nested.php';
echo 'nested defined: ', defined('REL_ONCE_NESTED') ? 'yes' : 'no', "\n";

// Already included: include_once returns TRUE and does not re-run the body.
$r = include_once 'rel_once_bare.php';
echo 'second include_once returned true: ', $r === true ? 'yes' : 'no', "\n";

// Same file via a different spelling stays deduped (realpath-canonical key).
include_once './rel_once_bare.php';
echo 'body ran exactly once: ', $GLOBALS['once_hits'] === 1 ? 'yes' : 'no', "\n";
?>
--EXPECT--
bare defined: yes
nested defined: yes
second include_once returned true: yes
body ran exactly once: yes
--CLEAN--
<?php
@unlink(__DIR__ . '/rel_once_bare.php');
@unlink(__DIR__ . '/rel_once_sub/nested.php');
@rmdir(__DIR__ . '/rel_once_sub');
?>
