<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Cuts src/ph7/vm_dom_big5.h -- the HTML document's Big5 index -- from php's
 * own HTML decoder and encoder.
 *
 *   XDEBUG_MODE=off php build-aux/gen_domenc_big5.php > src/ph7/vm_dom_big5.h
 *
 * Big5 is not the shape gen_domenc_dbcs.php emits and cannot be cut by it:
 * 1713 of its cells stand for a SUPPLEMENTARY code point and four for TWO
 * code points, so neither the cell table nor the encoder's key fits in the
 * sxu16 that file's tables are made of. Those 1717 cells live in a side table
 * the cell marks with 0xFFFF, and the encoder is cut in two halves so that
 * the common one keeps a 16-bit key.
 *
 * The oracle is `Dom\HTMLDocument` and nothing else, and each face is swept
 * from the door it serves: mbstring and iconv carry Big5 tables of their own
 * that answer differently, and php's decoder and encoder do not hold the same
 * characters either, so neither face is inverted out of the other.
 *
 * Nothing about the framing is assumed. Which bytes open a pair is measured
 * with the pair (b,0xA1) -- a lead eats the second byte and answers one
 * character, a non-lead answers two -- and asserted rather than emitted,
 * because the answer is the whole of 0x81..0xFE with no byte standing for a
 * character alone. Cells are measured per (lead,trail) BYTE pair rather than
 * through the standard's pointer arithmetic, which one encoding of this shape
 * has already been caught not following.
 *
 * DECODE is swept one lead per document, the cells comma-separated -- no trail
 * byte is 0x2C, and a broken pair re-reads only an ASCII byte, so the
 * separator survives every answer. ENCODE is swept through a COMMENT node,
 * which saveHtml() writes without escaping, so the bytes are the encoder's,
 * and it covers the WHOLE code space in chunks rather than the BMP, because
 * the supplementary cells are reachable from that face too.
 */
const T0 = 0x40, T1 = 0xFE;
const NCOL = T1 - T0 + 1;
const LEAD0 = 0x81, LEAD1 = 0xFE;
const NROW = LEAD1 - LEAD0 + 1;
const LABEL = 'Big5';

function cps(string $s): array {
	$o = [];
	for( $i = 0 ; $i < mb_strlen($s,'UTF-8') ; $i++ ){
		$o[] = mb_ord(mb_substr($s,$i,1,'UTF-8'),'UTF-8');
	}
	return $o;
}
function parts(string $src,int $want): array {
	$d = Dom\HTMLDocument::createFromString("<!doctype html><p>$src</p>",0,LABEL);
	$p = explode(',', $d->getElementsByTagName('p')[0]->textContent);
	if( count($p) - 1 !== $want ){
		fwrite(STDERR,sprintf("%s: %d parts for %d cells\n",LABEL,count($p) - 1,$want));
		exit(1);
	}
	return $p;
}
function fail(string $s): void { fwrite(STDERR,LABEL.": $s\n"); exit(1); }

/* --- the framing, asserted rather than emitted -------------------------- */
$src = ''; $bs = [];
for( $b = 0x80 ; $b <= 0xFF ; $b++ ){ $src .= chr($b).chr(0xA1).','; $bs[] = $b; }
$p = parts($src,count($bs));
foreach( $bs as $i => $b ){
	$c = cps($p[$i]);
	$bLead = $b >= LEAD0 && $b <= LEAD1;
	if( count($c) === 1 ){
		if( !$bLead ) fail(sprintf('byte %02X opens a pair and is outside 0x81..0xFE',$b));
		continue;
	}
	if( count($c) !== 2 ) fail(sprintf('byte %02X answered %d characters',$b,count($c)));
	if( $bLead ) fail(sprintf('byte %02X does not open a pair',$b));
	/* Every byte that is not a lead must be an error on its own: this index
	 * has no single-byte character above ASCII, and the C side spells that
	 * assumption rather than carrying a table for it. */
	if( $c[0] !== 0xFFFD ) fail(sprintf('byte %02X stands for U+%04X alone',$b,$c[0]));
}
/* --- the cells ---------------------------------------------------------- */
$cell = array_fill(0,NROW * NCOL,0);
$exIdx = []; $exA = []; $exB = [];
for( $L = LEAD0 ; $L <= LEAD1 ; $L++ ){
	$src = ''; $ts = [];
	for( $t = T0 ; $t <= T1 ; $t++ ){ $src .= chr($L).chr($t).','; $ts[] = $t; }
	$p = parts($src,count($ts));
	foreach( $ts as $i => $t ){
		$c = cps($p[$i]);
		if( count($c) === 0 || $c[0] === 0xFFFD ) continue;      /* no character here */
		if( count($c) > 2 ) fail(sprintf('cell %02X%02X answered %d characters',$L,$t,count($c)));
		$k = ($L - LEAD0) * NCOL + $t - T0;
		if( count($c) === 1 && $c[0] <= 0xFFFF ){
			if( $c[0] === 0xFFFF ) fail(sprintf('cell %02X%02X stands for U+FFFF, the side-table mark',$L,$t));
			$cell[$k] = $c[0];
			continue;
		}
		$cell[$k] = 0xFFFF;
		$exIdx[] = $k; $exA[] = $c[0]; $exB[] = $c[1] ?? 0;
		if( ($c[1] ?? 0) > 0xFFFF ) fail(sprintf('cell %02X%02X has a supplementary SECOND code point',$L,$t));
	}
}
/* --- the encoder, swept over the whole code space ----------------------- */
$d = Dom\HTMLDocument::createEmpty(LABEL);
$bCp = []; $bB = []; $sCp = []; $sB = [];
for( $base = 0x80 ; $base <= 0x10FFFF ; $base += 0x8000 ){
	$cpl = []; $txt = '';
	for( $cp = $base ; $cp < $base + 0x8000 && $cp <= 0x10FFFF ; $cp++ ){
		if( $cp >= 0xD800 && $cp <= 0xDFFF ) continue;
		$cpl[] = $cp; $txt .= mb_chr($cp,'UTF-8').',';
	}
	if( !$cpl ) continue;
	$p = explode(',', substr($d->saveHtml($d->createComment($txt)),4,-3));
	if( count($p) - 1 !== count($cpl) ) fail('encode sweep lost cells');
	foreach( $cpl as $i => $cp ){
		$g = $p[$i];
		if( $g === '?' ) continue;                               /* php cannot spell it */
		if( strlen($g) !== 2 ) fail(sprintf('U+%04X encoded to %d bytes',$cp,strlen($g)));
		$v = (ord($g[0]) << 8) | ord($g[1]);
		if( $cp <= 0xFFFF ){ $bCp[] = $cp; $bB[] = $v; } else { $sCp[] = $cp; $sB[] = $v; }
	}
}
fwrite(STDERR,sprintf("%s %5d cells (%d in the side table), %5d + %4d encoded\n",
	LABEL,count(array_filter($cell)),count($exIdx),count($bCp),count($sCp)));

/* --- emit --------------------------------------------------------------- */
$w = function(array $a,string $f = '0x%04X'){
	$o = ''; $a = array_values($a); $n = count($a); $per = $f === '0x%04X' ? 12 : 10;
	for( $i = 0 ; $i < $n ; $i++ ){
		$o .= ($i % $per === 0 ? "\t" : '').sprintf($f,$a[$i]).($i + 1 === $n ? '' : ',');
		if( $i % $per === $per - 1 || $i + 1 === $n ) $o .= "\n";
	}
	return $o;
};
echo <<<'TXT'
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The HTML document's Big5 index. GENERATED -- do not hand-edit; re-cut it
 * with `XDEBUG_MODE=off php build-aux/gen_domenc_big5.php`.
 *
 * This is NOT mbstring's or iconv's Big5, which answer differently, and the
 * two directions here are two tables because php's own two directions do not
 * hold the same characters.
 *
 *   aDomBig5Cell  the pair table, row (lead - 0x81), column (trail - 0x40),
 *                 holding the code point, 0 where there is no character, and
 *                 0xFFFF where the cell needs the side table below. It is
 *                 indexed by the BYTES rather than by the standard's pointer
 *                 because php's framing is not always the standard's.
 *   aDomBig5ExIdx the side table, ordered by cell index for a binary search,
 *   aDomBig5ExA   with the code point -- supplementary for all but four cells
 *   aDomBig5ExB   -- and a SECOND code point, or 0 where the cell has one.
 *   aDomBig5EncCp the ENCODER over the BMP, ordered by code point, with
 *   aDomBig5EncB  the lead<<8|trail pair it writes; and
 *   aDomBig5SupCp the same for the supplementary code points, whose key does
 *   aDomBig5SupB  not fit in sxu16.
 *
 * Nothing here is a single-byte answer: above ASCII this index has none, and
 * every byte of 0x81..0xFE opens a pair. That is measured when the table is
 * cut and spelled in the C rather than carried as a table.
 */

TXT;
printf("#define PH7_DOM_BIG5_EX  %d\n",count($exIdx));
printf("#define PH7_DOM_BIG5_ENC %d\n",count($bCp));
printf("#define PH7_DOM_BIG5_SUP %d\n\n",count($sCp));
printf("static const sxu16 aDomBig5Cell[%d * %d] = {\n%s};\n",NROW,NCOL,$w($cell));
printf("static const sxu16 aDomBig5ExIdx[PH7_DOM_BIG5_EX] = {\n%s};\n",$w($exIdx));
printf("static const sxu32 aDomBig5ExA[PH7_DOM_BIG5_EX] = {\n%s};\n",$w($exA,'0x%05X'));
printf("static const sxu16 aDomBig5ExB[PH7_DOM_BIG5_EX] = {\n%s};\n",$w($exB));
printf("static const sxu16 aDomBig5EncCp[PH7_DOM_BIG5_ENC] = {\n%s};\n",$w($bCp));
printf("static const sxu16 aDomBig5EncB[PH7_DOM_BIG5_ENC] = {\n%s};\n",$w($bB));
printf("static const sxu32 aDomBig5SupCp[PH7_DOM_BIG5_SUP] = {\n%s};\n",$w($sCp,'0x%05X'));
printf("static const sxu16 aDomBig5SupB[PH7_DOM_BIG5_SUP] = {\n%s};\n",$w($sB));
