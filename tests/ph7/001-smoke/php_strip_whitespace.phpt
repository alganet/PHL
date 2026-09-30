--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php_strip_whitespace(), and the two line-ending rules its token stream needs
--FILE--
<?php
$pswDir = sys_get_temp_dir() . '/phl_psw';
/* Every path in a diagnostic is this directory's; only the basename is stable.
 * php may name it with symlinks resolved (macOS's /var is /private/var). */
set_error_handler(function ($n, $s) use (&$pswDir) {
	$real = realpath($pswDir) ?: $pswDir;
	$s = str_replace([$real . '/', $real, $pswDir . '/', $pswDir], ['', '<dir>', '', '<dir>'], $s);
	/* why a DIRECTORY will not open is the system's errno: ENOENT here, EACCES on Windows */
	$s = preg_replace('/(\(<dir>\): Failed to open stream: ).*$/', '$1<os>', $s);
	echo '  ERR[', $n, '] ', $s, "\n";
	return true;
});
@mkdir($pswDir);
$pswCases = [
	/* whitespace collapses to ONE space; comments vanish entirely. */
	'plain'          => "<?php\n\$a = 1;\n\n\n\$b   =   2;\n",
	'block-tight'    => "<?php \$a/*c*/=1;\n",
	'block-spaced'   => "<?php \$a /*c*/ =1;\n",
	'line-hash'      => "<?php # c\necho 1;\n",
	'line-slash'     => "<?php // c\necho 1;\n",
	'doc'            => "<?php /** d */\necho 1;\n",
	'doc-tight'      => "<?php \$a/** d */=1;\n",
	'two-blocks'     => "<?php \$a/*c*//*d*/=1;\n",
	'trailing-line'  => "<?php echo 1; // end",
	/* the newline a heredoc terminator carries: in place of the whitespace after
	 * it, and otherwise after the token that follows it. */
	'heredoc-semi'   => "<?php\n\$x = <<<EOT\nbody\nEOT;\n\$y = 1;\n",
	'heredoc-nl'     => "<?php\n\$x = <<<EOT\nbody\nEOT\n;\n\$y = 1;\n",
	'heredoc-paren'  => "<?php\nf(<<<EOT\nbody\nEOT);\n\$y = 1;\n",
	'heredoc-comma'  => "<?php\n\$a = [<<<EOT\nb\nEOT, 2];\n",
	'heredoc-space'  => "<?php\n\$a = <<<EOT\nb\nEOT ? 1 : 2;\n",
	'heredoc-indent' => "<?php\n\$x = <<<EOT\n  indented\n  EOT;\n\$y=2;\n",
	'nowdoc'         => "<?php\n\$x = <<<'EOT'\nbody\nEOT;\n",
	/* inline HTML and the close tag are copied verbatim. */
	'html'           => "<h1>hi</h1>\n<?php echo 1; ?>\ntail\n",
	'empty'          => '',
	'no-php'         => "just text\n",
	/* a shebang line is eaten before the first token, as php's scanner eats it. */
	'shebang'        => "#!/usr/bin/env php\n<?php echo 1;\n",
	'shebang-only'   => "#!/bin/sh\n",
	/* CRLF: the open tag swallows the whole line ending, not just the CR. */
	'crlf'           => "<?php\r\n\$a = 1;\r\n",
	/* CR only (old Mac): a bare CR ends a line comment. */
	'cr-only'        => "<?php\r// one\r// two\rphpinfo();\r",
];
foreach ($pswCases as $pswName => $pswSrc) {
	$pswFile = $pswDir . '/' . $pswName . '.php';
	file_put_contents($pswFile, $pswSrc);
	/* The answers carry raw CR and LF bytes; escape them so the expectation is one
	 * line per case and no line-ending normalisation can touch it. */
	echo str_pad($pswName, 16), ': ',
		var_export(addcslashes(php_strip_whitespace($pswFile), "\r\n\t"), true), "\n";
	@unlink($pswFile);
}

/* The two token streams the rules above depend on. */
foreach (["<?php\r\n\$a = 1;\r\n", "<?php\r// one\rphpinfo();\r"] as $pswTokSrc) {
	echo 'tokens: ';
	foreach (token_get_all($pswTokSrc) as $pswTok) {
		echo is_array($pswTok)
			? token_name($pswTok[0]) . '(' . addcslashes($pswTok[1], "\r\n\t") . ') '
			: "CHAR($pswTok) ";
	}
	echo "\n";
}

/* A path nothing can be read from. */
echo 'missing: ', var_export(php_strip_whitespace($pswDir . '/no-such-thing.php'), true), "\n";
echo 'directory: ', var_export(php_strip_whitespace($pswDir), true), "\n";

try { php_strip_whitespace(''); }
catch (Throwable $e) { echo 'empty: ', get_class($e), ': ', $e->getMessage(), "\n"; }

@rmdir($pswDir);
restore_error_handler();
?>
--EXPECT--
plain           : '<?php\\n$a = 1; $b = 2; '
block-tight     : '<?php $a=1; '
block-spaced    : '<?php $a =1; '
line-hash       : '<?php  echo 1; '
line-slash      : '<?php  echo 1; '
doc             : '<?php  echo 1; '
doc-tight       : '<?php $a=1; '
two-blocks      : '<?php $a=1; '
trailing-line   : '<?php echo 1; '
heredoc-semi    : '<?php\\n$x = <<<EOT\\nbody\\nEOT;\\n$y = 1; '
heredoc-nl      : '<?php\\n$x = <<<EOT\\nbody\\nEOT\\n; $y = 1; '
heredoc-paren   : '<?php\\nf(<<<EOT\\nbody\\nEOT)\\n; $y = 1; '
heredoc-comma   : '<?php\\n$a = [<<<EOT\\nb\\nEOT,\\n2]; '
heredoc-space   : '<?php\\n$a = <<<EOT\\nb\\nEOT\\n? 1 : 2; '
heredoc-indent  : '<?php\\n$x = <<<EOT\\n  indented\\n  EOT;\\n$y=2; '
nowdoc          : '<?php\\n$x = <<<\'EOT\'\\nbody\\nEOT;\\n'
html            : '<h1>hi</h1>\\n<?php echo 1; ?>\\ntail\\n'
empty           : ''
no-php          : 'just text\\n'
shebang         : '<?php echo 1; '
shebang-only    : ''
crlf            : '<?php\\r\\n$a = 1; '
cr-only         : '<?php\\r phpinfo(); '
tokens: T_OPEN_TAG(<?php\r\n) T_VARIABLE($a) T_WHITESPACE( ) CHAR(=) T_WHITESPACE( ) T_LNUMBER(1) CHAR(;) T_WHITESPACE(\r\n) 
tokens: T_OPEN_TAG(<?php\r) T_COMMENT(// one) T_WHITESPACE(\r) T_STRING(phpinfo) CHAR(() CHAR()) CHAR(;) T_WHITESPACE(\r) 
missing:   ERR[2] php_strip_whitespace(no-such-thing.php): Failed to open stream: No such file or directory
''
directory:   ERR[2] php_strip_whitespace(<dir>): Failed to open stream: <os>
''
empty: ValueError: Path must not be empty
