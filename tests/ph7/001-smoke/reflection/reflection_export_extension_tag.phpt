--TEST--
Reflection: the export format NAMES the extension in its <internal:...> tag
--FILE--
<?php
// php's export head opens with `<internal:<extension>>`, and this engine wrote
// the literal `internal:Core` for all 878 functions and 197 classes because it
// had no partition to name one from. Only heads both engines spell identically
// are pinned here.
foreach (['json_encode', 'strlen', 'preg_quote', 'bcadd', 'token_name',
          'session_id', 'iconv_strlen', 'cal_to_jd', 'mb_strlen', 'array_map'] as $reflXFn) {
    echo strtok((string)new ReflectionFunction($reflXFn), "\n"), "\n";
}
foreach (['JsonSerializable', 'DateTimeInterface', 'SessionHandlerInterface',
          'PhpToken', 'stdClass', 'BcMath\\Number'] as $reflXCl) {
    echo strtok((string)new ReflectionClass($reflXCl), "\n"), "\n";
}
// A METHOD names its DECLARING class's extension.
foreach ([['ArrayObject', 'count'], ['PhpToken', 'getTokenName'],
          ['SessionHandler', 'open']] as [$reflXC, $reflXM]) {
    echo trim(strtok((string)new ReflectionMethod($reflXC, $reflXM), "\n")), "\n";
}
// A userland target is php's `<user>`, extension or not.
function reflXUserFn() {}
echo strtok((string)new ReflectionFunction('reflXUserFn'), "\n"), "\n";
--EXPECT--
Function [ <internal:json> function json_encode ] {
Function [ <internal:Core> function strlen ] {
Function [ <internal:pcre> function preg_quote ] {
Function [ <internal:bcmath> function bcadd ] {
Function [ <internal:tokenizer> function token_name ] {
Function [ <internal:session> function session_id ] {
Function [ <internal:iconv> function iconv_strlen ] {
Function [ <internal:calendar> function cal_to_jd ] {
Function [ <internal:mbstring> function mb_strlen ] {
Function [ <internal:standard> function array_map ] {
Interface [ <internal:json> interface JsonSerializable ] {
Interface [ <internal:date> interface DateTimeInterface ] {
Interface [ <internal:session> interface SessionHandlerInterface ] {
Class [ <internal:tokenizer> class PhpToken implements Stringable ] {
Class [ <internal:Core> class stdClass ] {
Class [ <internal:bcmath> final readonly class BcMath\Number implements Stringable ] {
Method [ <internal:SPL, prototype Countable> public method count ] {
Method [ <internal:tokenizer> public method getTokenName ] {
Method [ <internal:session, prototype SessionHandlerInterface> public method open ] {
Function [ <user> function reflXUserFn ] {
