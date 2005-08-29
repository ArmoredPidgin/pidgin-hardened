#!/usr/bin/perl

# Copyright 2003-2004 Nathan Walp <faceprint@faceprint.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 50 Temple Place, Suite 330, Boston, MA 02111-1307  USA
#


my $PACKAGE="gaim";


use Locale::Language;

$lang{en_AU} = "English (Australian)";
$lang{en_CA} = "English (Canadian)";
$lang{en_GB} = "English (British)";
$lang{pt_BR} = "Portuguese (Brazilian)";
$lang{'sr@Latn'} = "Serbian (Latin)";
$lang{zh_CN} = "Chinese (Simplified)";
$lang{zh_TW} = "Chinese (Traditional)";

opendir(DIR, ".") || die "can't open directory: $!";
@pos = grep { /\.po$/ && -f } readdir(DIR);
foreach (@pos) { s/\.po$//; };
closedir DIR;

@pos = sort @pos;

$now = `date`;

system("./update.pl --pot > /dev/null");

$_ = `msgfmt --statistics $PACKAGE.pot -o /dev/null 2>&1`;

die "unable to get total: $!" unless (/(\d+) untranslated messages/);

$total = $1;

print "<?xml version='1.0'?>\n";
print "<?xml-stylesheet type='text/xsl' href='l10n.xsl'?>\n";
print "<project version='1.0' xmlns:l10n='http://faceprint.com/code/l10n' name='$PACKAGE' pofile='$PACKAGE.pot' strings='$total'>\n";

foreach $index (0 .. $#pos) {
	$trans = $fuzz = $untrans = 0;
	$po = $pos[$index];
	print STDERR "$po..." if($ARGV[0] eq '-v');
	system("msgmerge $po.po $PACKAGE.pot -o $po.new 2>/dev/null");
	$_ = `msgfmt --statistics $po.new -o /dev/null 2>&1`;
	chomp;
	if(/(\d+) translated message/) { $trans = $1; }
	if(/(\d+) fuzzy translation/) { $fuzz = $1; }
	if(/(\d+) untranslated message/) { $untrans = $1; }
	unlink("$po.new");

	print "<lang code='$po' translated='$trans' fuzzy='$fuzz' />\n";
	print STDERR "done ($untrans untranslated strings).\n" if($ARGV[0] eq '-v');
}

print "</project>\n";

