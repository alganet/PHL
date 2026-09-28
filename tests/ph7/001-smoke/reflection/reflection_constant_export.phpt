--TEST--
Reflection: ReflectionConstant's export is php's one-line Constant block
--FILE--
<?php
// `(string)new ReflectionConstant(...)` printed `Constant [ NAME ]` here and
// nothing else -- no flag word, no type, no value -- where php writes one line
// carrying all three, and where isDeprecated() answered false for every name.
define('ReflCUInt', 5);
define('ReflCUStr', 'ab');
define('ReflCUNull', null);
define('ReflCUArr', [1, 2]);
define('ReflCUTrue', true);
define('ReflCUFalse', false);
const ReflCUFloat = 1.5;
// (no PHP_OS_FAMILY here: its VALUE is the platform's, and the Windows gate
// runs the same expectation)
foreach (['JSON_HEX_TAG', 'PHP_EOL', 'PHP_INT_MAX', 'M_PI', 'E_ALL',
          'SORT_REGULAR', 'DATE_RFC7231', 'FILE_TEXT', 'FILE_BINARY', 'MT_RAND_PHP',
          'DOM_PHP_ERR', 'CURLOPT_BINARYTRANSFER', 'PHP_SAPI',
          'ReflCUInt', 'ReflCUStr', 'ReflCUNull', 'ReflCUArr', 'ReflCUTrue',
          'ReflCUFalse', 'ReflCUFloat'] as $reflCName) {
    $reflCR = new ReflectionConstant($reflCName);
    echo str_replace("\n", '\n', (string)$reflCR),
         ' isDeprecated=', var_export($reflCR->isDeprecated(), true), "\n";
}
// Reading the export does NOT raise the notice that NAMING one would.
set_error_handler(function ($reflCN, $reflCS) { echo "UNEXPECTED: $reflCS\n"; return true; });
(string)new ReflectionConstant('DATE_RFC7231');
(new ReflectionConstant('MT_RAND_PHP'))->isDeprecated();
restore_error_handler();
echo "quiet\n";
--EXPECT--
Constant [ <persistent> int JSON_HEX_TAG ] { 1 }\n isDeprecated=false
Constant [ <persistent> string PHP_EOL ] { \n }\n isDeprecated=false
Constant [ <persistent> int PHP_INT_MAX ] { 9223372036854775807 }\n isDeprecated=false
Constant [ <persistent> float M_PI ] { 3.1415926535898 }\n isDeprecated=false
Constant [ <persistent> int E_ALL ] { 30719 }\n isDeprecated=false
Constant [ <persistent> int SORT_REGULAR ] { 0 }\n isDeprecated=false
Constant [ <persistent, deprecated> string DATE_RFC7231 ] { D, d M Y H:i:s \G\M\T }\n isDeprecated=true
Constant [ <persistent, deprecated> int FILE_TEXT ] { 0 }\n isDeprecated=true
Constant [ <persistent, deprecated> int FILE_BINARY ] { 0 }\n isDeprecated=true
Constant [ <persistent, deprecated> int MT_RAND_PHP ] { 1 }\n isDeprecated=true
Constant [ <persistent, deprecated> int DOM_PHP_ERR ] { 0 }\n isDeprecated=true
Constant [ <persistent, deprecated> int CURLOPT_BINARYTRANSFER ] { 19914 }\n isDeprecated=true
Constant [ <persistent, no_file_cache> string PHP_SAPI ] { cli }\n isDeprecated=false
Constant [ int ReflCUInt ] { 5 }\n isDeprecated=false
Constant [ string ReflCUStr ] { ab }\n isDeprecated=false
Constant [ null ReflCUNull ] {  }\n isDeprecated=false
Constant [ array ReflCUArr ] { Array }\n isDeprecated=false
Constant [ bool ReflCUTrue ] { 1 }\n isDeprecated=false
Constant [ bool ReflCUFalse ] {  }\n isDeprecated=false
Constant [ float ReflCUFloat ] { 1.5 }\n isDeprecated=false
quiet
