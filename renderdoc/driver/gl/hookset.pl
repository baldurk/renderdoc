#!/bin/perl

use POSIX;

sub trim {
    (my $s = $_[0]) =~ s/^\s+|\s+$//g;
    return $s;        
}

my $printdefs = $ARGV[$1] eq "defs";

open(HOOKSET, "<gl_hookset.h") || die "Couldn't open gl_hookset.h - run in driver/gl/";

my @dllexport = ();
my @glext = ();

my $mode = "";

while(<HOOKSET>)
{
	my $line = $_;

	if($line =~ /\/\/ --/)
	{
		$mode = "";
	}
	elsif($line =~ /\/\/ \+\+ ([a-z]*)/)
	{
		$mode = "dllexport" if $1 eq "dllexport"; 
		$mode = "glext" if $1 eq "glext";
	}
	elsif($line =~ /^\s*\/\/ .*/)
	{
		# skip comments
	}
	elsif($line =~ /^\s*$/)
	{
		# skip blank lines
	}
	elsif($mode ne "")
	{
		if($line =~ /(PFN.*PROC) (.*);( \/\/ aliases )?([a-zA-Z0-9_ ,]*)?/)
		{
			my $typedef = $1;
			my $name = $2;
			my $aliases = $4;

			my $def = trim(`grep -h $typedef official/*`);

			if($def =~ /^typedef (.*)\([A-Z *]* $typedef\) \((.*)\);/)
			{
				my $returnType = trim($1);
				my $args = $2;
				$args = "" if $args eq "void";
				my $origargs = $args;
				$args =~ s/ *([a-zA-Z_][a-zA-Z_0-9]*)(,|\Z)/, $1$2/g;

				my $argcount = () = $args =~ /,/g; 

				$argcount = floor(($argcount + 1)/2);

				my $funcdefmacro = "HookWrapper$argcount($returnType, $name";
				$funcdefmacro .= ", $args" if $args ne "";
				$funcdefmacro .= ");";

				my %func = ('name', $name, 'typedef', $typedef, 'macro', $funcdefmacro, 'ret', $returnType, 'args', $origargs, 'aliases', $aliases);

				push @dllexport, { %func } if $mode eq "dllexport";
				push @glext, { %func } if $mode eq "glext";
			}
			else
			{
				print "MALFORMED $mode DEFINITION OR NO DEFINITION FOUND FOR $typedef: '$def'\n";
			}
		}
		else
		{
			print "MALFORMED $mode LINE IN gl_hookset.h: '$line'\n";
		}
	}
}

close(HOOKSET);

if($printdefs)
{
	foreach my $el (@dllexport)
	{
		print "        IMPLEMENT_FUNCTION_SERIALISED($el->{ret}, $el->{name}($el->{args}));\n";
	}
	print "\n";
	foreach my $el (@glext)
	{
		print "        IMPLEMENT_FUNCTION_SERIALISED($el->{ret}, $el->{name}($el->{args}));\n";
	}
	print "\n";
	exit;
}

print "////////////////////////////////////////////////////\n";
print "\n";
print "// dllexport functions\n";
print "#define DLLExportHooks() \\\n";
foreach my $el (@dllexport)
{
	print "    HookInit($el->{name}); \\\n"
}
print "\n";
print "\n";
print "\n";
print "// gl extensions\n";
print "#define HookCheckGLExtensions() \\\n";
foreach my $el (@glext)
{
	print "    HookExtension($el->{typedef}, $el->{name}); \\\n";
	foreach(split(/, */, $el->{aliases}))
	{
		print "    HookExtensionAlias($el->{typedef}, $el->{name}, $_); \\\n";
	}
}
foreach my $el (@dllexport)
{
	print "    HookExtension($el->{typedef}, $el->{name}); \\\n"
}
print "\n";
print "\n";
print "\n";
print "// dllexport functions\n";
print "#define DefineDLLExportHooks() \\\n";
foreach my $el (@dllexport)
{
	print "    $el->{macro} \\\n"
}
print "\n";
print "\n";
print "\n";
print "// gl extensions\n";
print "#define DefineGLExtensionHooks() \\\n";
foreach my $el (@glext)
{
	print "    $el->{macro} \\\n"
}
print "\n";
print "\n";
print "\n";
print "\n";
