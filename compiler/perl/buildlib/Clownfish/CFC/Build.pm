# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

use strict;
use warnings;

package Clownfish::CFC::Build;

our $VERSION = '0.004000';
$VERSION = eval $VERSION;

# In order to find Clownfish::CFC::Perl::Build::Charmonic, look in 'lib'
# and cleanup @INC afterwards.
use lib 'lib';
use base qw( Clownfish::CFC::Perl::Build::Charmonic );
no lib 'lib';

use File::Spec::Functions qw( catfile updir catdir curdir );
use File::Copy qw( move );
use File::Path qw( rmtree );
use File::Find qw( find );
use Config;
use Cwd qw( getcwd );
use Carp;

# Establish the filepaths for various assets.  If the file `LICENSE` is found
# in the current working directory, this is a CPAN distribution rather than a
# checkout from version control and things live in different dirs.
my $CHARMONIZER_C;
my $INCLUDE;
my $CFC_SOURCE_DIR;
my $IS_CPAN = -e 'LICENSE';
if ($IS_CPAN) {
    $CHARMONIZER_C  = 'charmonizer.c';
    $INCLUDE        = 'include';
    $CFC_SOURCE_DIR = 'src';
}
else {
    $CHARMONIZER_C = catfile( updir(), 'common', 'charmonizer.c' );
    $INCLUDE        = catdir( updir(), 'include' );
    $CFC_SOURCE_DIR = catdir( updir(), 'src' );
}
my $PPPORT_H_PATH  = catfile( $INCLUDE,   'ppport.h' );

sub new {
    my ( $class, %args ) = @_;
    #$args{c_source} = $CFC_SOURCE_DIR;
    $args{include_dirs} ||= [];
    my @aux_include = (
        $INCLUDE,
        $CFC_SOURCE_DIR,
        curdir(),    # for charmony.h
    );
    push @{ $args{include_dirs} }, @aux_include;
    return $class->SUPER::new(
        %args,
        recursive_test_files => 1,
        charmonizer_params   => {
            charmonizer_c => $CHARMONIZER_C,
        },
        extra_compiler_flags => ['-DCFCPERL'],
        extra_linker_flags => ['-L.', '-lcfc'],
    );
}

# Write ppport.h, which supplies some XS routines not found in older Perls and
# allows us to use more up-to-date XS API while still supporting Perls back to
# 5.8.3.
#
# The Devel::PPPort docs recommend that we distribute ppport.h rather than
# require Devel::PPPort itself, but ppport.h isn't compatible with the Apache
# license.
sub ACTION_ppport {
    my $self = shift;
    if ( !-e $PPPORT_H_PATH ) {
        require Devel::PPPort;
        $self->add_to_cleanup($PPPORT_H_PATH);
        Devel::PPPort::WriteFile($PPPORT_H_PATH);
    }
}

sub ACTION_charmony {
    my $self = shift;
    $self->depends_on('ppport');
    $self->SUPER::ACTION_charmony;
}

sub ACTION_static_lib {
    my $self = shift;
    $self->depends_on(qw( charmony ));
    system("make", '-j', "static") and die "Can't make static lib for CFC";
}

sub ACTION_code {
    my $self = shift;

    $self->depends_on(qw( static_lib ));

    my @flags = $self->split_like_shell($self->charmony("EXTRA_CFLAGS"));
    # The flag for the MSVC6 hack contains spaces. Make sure it stays quoted.
    @flags = map { /\s/ ? qq{"$_"} : $_ } @flags;

    $self->extra_compiler_flags( '-DCFCPERL', @flags );

    $self->SUPER::ACTION_code;
}

sub ACTION_realclean {
    my $self = shift;
    if (-e 'Makefile') {
        system("make", "distclean");
    }
    $self->SUPER::ACTION_realclean;
}

sub _valgrind_base_command {
    return
          "PERL_DESTRUCT_LEVEL=2 LUCY_VALGRIND=1 valgrind "
        . "--leak-check=yes "
        . "--show-reachable=yes "
        . "--dsymutil=yes "
        . "--suppressions=../../devel/conf/cfcompiler-perl.supp ";
}

# Run the entire test suite under Valgrind.
#
# For this to work, Lucy must be compiled with the LUCY_VALGRIND environment
# variable set to a true value, under a debugging Perl.
#
# A custom suppressions file will probably be needed -- use your judgment.
# To pass in one or more local suppressions files, provide a comma separated
# list like so:
#
#   $ ./Build test_valgrind --suppressions=foo.supp,bar.supp
sub ACTION_test_valgrind {
    my $self = shift;
    # Debian's debugperl uses the Config.pm of the standard system perl
    # so -DDEBUGGING won't be detected.
    die "Must be run under a perl that was compiled with -DDEBUGGING"
        unless $self->config('ccflags') =~ /-D?DEBUGGING\b/
               || $^X =~ /\bdebugperl\b/;
    if ( !$ENV{LUCY_VALGRIND} ) {
        warn "\$ENV{LUCY_VALGRIND} not true -- possible false positives";
    }
    $self->depends_on('code');

    # Unbuffer STDOUT, grab test file names and suppressions files.
    $|++;
    my $t_files = $self->find_test_files;    # not public M::B API, may fail
    my $valgrind_command = $self->_valgrind_base_command;

    if ( my $local_supp = $self->args('suppressions') ) {
        for my $supp ( split( ',', $local_supp ) ) {
            $valgrind_command .= "--suppressions=$supp ";
        }
    }

    # Iterate over test files.
    my @failed;
    for my $t_file (@$t_files) {

        # Run test file under Valgrind.
        print "Testing $t_file...";
        die "Can't find '$t_file'" unless -f $t_file;
        my $command = "$valgrind_command $^X -Mblib $t_file 2>&1";
        my $output = "\n" . ( scalar localtime(time) ) . "\n$command\n";
        $output .= `$command`;

        # Screen-scrape Valgrind output, looking for errors and leaks.
        if (   $?
            or $output =~ /ERROR SUMMARY:\s+[^0\s]/
            or $output =~ /definitely lost:\s+[^0\s]/
            or $output =~ /possibly lost:\s+[^0\s]/
            or $output =~ /still reachable:\s+[^0\s]/ )
        {
            print " failed.\n";
            push @failed, $t_file;
            print "$output\n";
        }
        else {
            print " succeeded.\n";
        }
    }

    # If there are failed tests, print a summary list.
    if (@failed) {
        print "\nFailed "
            . scalar @failed . "/"
            . scalar @$t_files
            . " test files:\n    "
            . join( "\n    ", @failed ) . "\n";
        exit(1);
    }
}

sub ACTION_dist {
    my $self = shift;

    # We build our Perl release tarball from a subdirectory rather than from
    # the top-level $REPOS_ROOT.  Because some assets we need are outside this
    # directory, we need to copy them in.
    my %to_copy = (
        '../../CONTRIBUTING' => 'CONTRIBUTING',
        '../../LICENSE'      => 'LICENSE',
        '../../NOTICE'       => 'NOTICE',
        '../../README'       => 'README',
        '../../lemon'        => 'lemon',
        '../src'             => 'src',
        '../include'         => 'include',
        $CHARMONIZER_C       => 'charmonizer.c',
    );
    print "Copying files...\n";
    while (my ($from, $to) = each %to_copy) {
        confess("'$to' already exists") if -e $to;
        system("cp -R $from $to") and confess("cp failed");
    }
    move( "MANIFEST", "MANIFEST.bak" ) or die "move() failed: $!";
    my $saved = _hide_pod( $self, { 'lib/Clownfish/CFC.pod' => 1 } );
    $self->depends_on("manifest");
    $self->SUPER::ACTION_dist;
    _restore_pod( $self, $saved );

    # Now that the tarball is packaged up, delete the copied assets.
    rmtree($_) for values %to_copy;
    unlink("META.yml");
    unlink("META.json");
    move( "MANIFEST.bak", "MANIFEST" ) or die "move() failed: $!";
}

# Strip POD from files in the `lib` directory.  This is a temporary measure to
# allow us to release Clownfish as a separate dist but with a cloaked API.
sub _hide_pod {
    my ( $self, $excluded ) = @_;
    my %saved;
    find(
        {
            no_chdir => 1,
            wanted   => sub {
                my $path = $File::Find::name;
                return if $excluded->{$path};
                return unless $path =~ /\.(pm|pod)$/;
                open( my $fh, '<:encoding(UTF-8)', $path )
                  or confess("Can't open '$path' for reading: $!");
                my $content = do { local $/; <$fh> };
                close $fh;
                if ( $path =~ /\.pod$/ ) {
                    $saved{$path} = $content;
                    print "Hiding POD for $path\n";
                    unlink($path) or confess("Can't unlink '$path': $!");
                }
                else {
                    my $copy = $content;
                    $copy =~ s/^=\w+.*?^=cut\s*$//gsm;
                    return if $copy eq $content;
                    print "Hiding POD for $path\n";
                    $saved{$path} = $content;
                    open( $fh, '>:encoding(UTF-8)', $path )
                      or confess("Can't open '$path' for writing: $!");
                    print $fh $copy;
                }
            },
        },
        'lib',
    );
    return \%saved;
}

# Undo POD hiding.
sub _restore_pod {
    my ( $self, $saved ) = @_;
    while ( my ( $path, $content ) = each %$saved ) {
        open( my $fh, '>:encoding(UTF-8)', $path )
          or confess("Can't open '$path' for writing: $!");
        print $fh $saved->{$path};
    }
}

1;

