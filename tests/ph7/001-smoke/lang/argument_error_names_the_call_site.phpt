--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An argument diagnostic names the CALL SITE's file and line, not the entry script
--FILE--
<?php
$argSiteDir = sys_get_temp_dir() . '/phl_arg_site';
@mkdir($argSiteDir);
$argSiteLib = $argSiteDir . '/arg_site_lib.php';
file_put_contents($argSiteLib, "<?php\n"                              // 1
	. "function argSiteTwo(\$a, \$b) {}\n"                            // 2
	. "function argSiteInt(int \$x) {}\n"                             // 3
	. "class ArgSiteK { public function m(int \$x) {} }\n"             // 4
	. "function argSiteCallFew() { argSiteTwo(1); }\n"                // 5
	. "function argSiteCallType() { argSiteInt('nope'); }\n"          // 6
	. "function argSiteCallMethod() { (new ArgSiteK)->m([]); }\n");   // 7
require $argSiteLib;

/* Raised inside the library: php names the library's file and the line the CALL
 * is on -- 5, 6 and 7 -- not the script that required it. */
foreach (['argSiteCallFew', 'argSiteCallType', 'argSiteCallMethod'] as $f) {
	try { $f(); } catch (Throwable $e) {
		/* The path is absolute and its separator is the platform's, so the
		 * directory is normalised away and only the basename is compared. */
		echo get_class($e), ': ',
			preg_replace('#\S*[\\\\/]arg_site_lib\.php#', 'arg_site_lib.php', $e->getMessage()), "\n";
	}
}

/* Called from HERE, the same two name this file and the line below. */
try { argSiteTwo(1); } catch (Throwable $e) { echo basename($e->getFile()), ' | ', preg_replace('/ in \S+ on line (\d+)/', ' in <this> on line $1', $e->getMessage()), "\n"; }
try { argSiteInt([]); } catch (Throwable $e) { echo basename($e->getFile()), ' | ', preg_replace('/called in \S+ on line (\d+)/', 'called in <this> on line $1', $e->getMessage()), "\n"; }

@unlink($argSiteLib);
@rmdir($argSiteDir);
?>
--EXPECT--
ArgumentCountError: Too few arguments to function argSiteTwo(), 1 passed in arg_site_lib.php on line 5 and exactly 2 expected
TypeError: argSiteInt(): Argument #1 ($x) must be of type int, string given, called in arg_site_lib.php on line 6
TypeError: ArgSiteK::m(): Argument #1 ($x) must be of type int, array given, called in arg_site_lib.php on line 7
arg_site_lib.php | Too few arguments to function argSiteTwo(), 1 passed in <this> on line 26 and exactly 2 expected
arg_site_lib.php | argSiteInt(): Argument #1 ($x) must be of type int, array given, called in <this> on line 27
