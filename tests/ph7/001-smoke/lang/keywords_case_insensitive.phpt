--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP keywords are case-insensitive in every spelling
--FILE--
<?php
// Control flow.
IF (TRUE) { echo "if "; } ELSEIF (false) { echo "no"; } ELSE { echo "no"; }
if (false) { echo "no"; } ElseIf (true) { echo "elseif "; }
$kciI = 0; WHILE ($kciI < 1) { echo "while "; $kciI++; }
$kciI = 0; DO { echo "do "; $kciI++; } While ($kciI < 1);
FOR ($kciI = 0; $kciI < 1; $kciI++) { echo "for "; }
FOREACH ([1] AS $kciV) { echo "foreach "; }
SWITCH (1) { CASE 1: echo "switch "; BREAK; DEFAULT: echo "no"; }
foreach ([1, 2] as $kciV) { IF ($kciV > 1) { BREAK; } CONTINUE; }
echo "\n";

// Alternative (colon) syntax, whose closers are keywords too.
IF (true): echo "endif "; ENDIF;
$kciI = 0; WHILE ($kciI < 1): echo "endwhile "; $kciI++; ENDWHILE;
FOR ($kciI = 0; $kciI < 1; $kciI++): echo "endfor "; ENDFOR;
FOREACH ([1] AS $kciV): echo "endforeach "; ENDFOREACH;
SWITCH (1): CASE 1: echo "endswitch "; ENDSWITCH;
echo "\n";

// Declarations and OO.
ABSTRACT CLASS KciBase
{
    PUBLIC CONST C = 'const ';
    PROTECTED STATIC $shared = 'static-prop ';
    PRIVATE $hidden = 'private ';
    ABSTRACT PUBLIC FUNCTION shape(): STRING;
    PUBLIC STATIC FUNCTION make(): STATIC { RETURN NEW STATIC(); }
    PUBLIC FUNCTION peek(): string { RETURN SELF::C . $this->hidden . STATIC::$shared; }
}
INTERFACE KciShape { CONST TAG = 'iface '; }
TRAIT KciTrait { PUBLIC FUNCTION tagged(): string { RETURN 'trait '; } }
FINAL CLASS KciImpl EXTENDS KciBase IMPLEMENTS KciShape
{
    USE KciTrait;
    PUBLIC FUNCTION shape(): STRING { RETURN PARENT::C . 'parent '; }
}
$kciO = KciImpl::make();
echo get_class($kciO), ' ', $kciO->peek(), $kciO->shape(), $kciO->tagged(), KciImpl::TAG, "\n";
echo var_export($kciO INSTANCEOF KciShape, TRUE), var_export($kciO INSTANCEOF KciBase, TRUE), "\n";

// Functions, statics, globals, references, variadics.
FUNCTION kciCounter(): INT { STATIC $n = 0; RETURN ++$n; }
$kciG = 'global ';
FUNCTION kciReadGlobal(): string { GLOBAL $kciG; RETURN $kciG; }
FUNCTION kciSum(INT ...$parts): INT { RETURN array_sum($parts); }
echo kciCounter(), kciCounter(), ' ', kciReadGlobal(), kciSum(1, 2, 3), "\n";

// Closures, arrow functions, first-class callables, match, enums, generators.
$kciClosure = STATIC FUNCTION (INT $x): INT { RETURN $x * 2; };
$kciArrow = FN(INT $x): INT => $x + 1;
$kciFcc = strtoupper(...);
echo $kciClosure(2), $kciArrow(1), $kciFcc('fcc'), ' ', MATCH (2) { 1 => 'no', 2 => 'match ', DEFAULT => 'no' };
ENUM KciSuit: string { CASE Hearts = 'H'; CASE Spades = 'S'; }
echo KciSuit::Hearts->value, KciSuit::FROM('S')->name, ' ';
FUNCTION kciGen() { YIELD 1; YIELD FROM [2, 3]; }
FOREACH (kciGen() AS $kciV) { echo $kciV; }
echo "\n";

// Expressions: casts, language constructs, error handling, clone, string ops.
$kciArr = ARRAY('k' => 'v');
echo var_export((INT) '5', TRUE), ' ', var_export((FLOAT) '1.5', TRUE), ' ',
    var_export((BOOL) 1, TRUE), ' ', var_export((STRING) 2, TRUE), ' ',
    var_export((ARRAY) 1, TRUE), ' ', get_class((OBJECT) $kciArr), ((OBJECT) $kciArr)->k, "\n";
echo var_export(ISSET($kciArr['k']), TRUE), var_export(EMPTY($kciArr), TRUE),
    var_export(ISSET($kciMissing), TRUE), "\n";
LIST($kciA, $kciB) = [1, 2];
[$kciC] = [3];
echo $kciA, $kciB, $kciC, ' ';
UNSET($kciA);
echo var_export(ISSET($kciA), TRUE), "\n";
PRINT "print ";
EVAL('echo "eval ";');
TRY {
    THROW NEW RuntimeException('thrown');
} CATCH (RuntimeException $kciE) {
    echo 'caught:', $kciE->GETMESSAGE(), ' ';
} FINALLY {
    echo "finally ";
}
$kciClone = CLONE $kciO;
echo var_export($kciClone !== $kciO, TRUE), var_export(TRUE AND TRUE, TRUE),
    var_export(FALSE OR TRUE, TRUE), var_export(TRUE XOR TRUE, TRUE), ' ', NULL ?? 'fallback', "\n";

// A reserved word used as a member NAME is a name, and case-SENSITIVE like every
// class constant — the keyword fold must not reach it.
CLASS KciMembers
{
    CONST STATIC = 'STATIC';
    CONST SELF = 'SELF';
    CONST ISSET = 'ISSET';
    CONST Static = 'Static';
    PUBLIC FUNCTION Static(): string { RETURN 'method-Static'; }
}
echo KciMembers::STATIC, ' ', KciMembers::SELF, ' ', KciMembers::ISSET, ' ',
    KciMembers::Static, ' ', (NEW KciMembers)->Static(), "\n";

// The keyword-shaped spellings are still names where php says they are names.
define('KCI_OBJECT', 'defined-const');
echo KCI_OBJECT, "\n";
CLASS KciNames { PUBLIC $list = 'prop'; PUBLIC FUNCTION match(): string { RETURN 'method'; } }
$kciN = NEW KciNames();
echo $kciN->list, ' ', $kciN->match(), ' ', KciNames::CLASS, "\n";
?>
--EXPECT--
if elseif while do for foreach switch 
endif endwhile endfor endforeach endswitch 
KciImpl const private static-prop const parent trait iface 
truetrue
12 global 6
42FCC match HSpades 123
5 1.5 true '2' array (
  0 => 1,
) stdClassv
truefalsefalse
123 false
print eval caught:thrown finally truetruetruefalse fallback
STATIC SELF ISSET Static method-Static
defined-const
prop method KciNames
--CLEAN--
<?php
