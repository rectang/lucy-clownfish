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

# In order to find Clownfish::CFC::Perl::Build::Charmonic, look in 'lib'
# and cleanup @INC afterwards.
use lib 'lib';
use base qw( Clownfish::CFC::Perl::Build::Charmonic );
no lib 'lib';

use File::Spec::Functions qw( catfile updir catdir );
use Config;
use Cwd qw( getcwd );
use Carp;

my $base_dir = catdir( updir(), updir() );
my $COMMON_SOURCE_DIR = catdir( updir(), 'common' );
my $CHARMONIZER_C     = catfile( $COMMON_SOURCE_DIR, 'charmonizer.c' );
my $PPPORT_H_PATH = catfile( updir(), qw( include ppport.h ) );
my $LEMON_DIR = catdir( $base_dir, 'lemon' );
my $LEMON_EXE_PATH = catfile( $LEMON_DIR, "lemon$Config{_exe}" );
my $CFC_SOURCE_DIR = catdir( updir(), 'src' );

sub new {
    my ( $class, %args ) = @_;
    return $class->SUPER::new(
        %args,
        recursive_test_files => 1,
        charmonizer_params   => {
            charmonizer_c => $CHARMONIZER_C,
        },
    );
}

sub _run_make {
    my ( $self, %params ) = @_;
    my @command           = @{ $params{args} };
    my $dir               = $params{dir};
    my $current_directory = getcwd();
    chdir $dir if $dir;
    unshift @command, 'CC=' . $self->config('cc');
    if ( $self->config('cc') =~ /^cl\b/ ) {
        unshift @command, "-f", "Makefile.MSVC";
    }
    elsif ( $^O =~ /mswin/i ) {
        unshift @command, "-f", "Makefile.MinGW";
    }
    unshift @command, "$Config{make}";
    system(@command) and confess("$Config{make} failed");
    chdir $current_directory if $dir;
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

# Build the Lemon parser generator.
sub ACTION_lemon {
    my $self = shift;
    print "Building the Lemon parser generator...\n\n";
    $self->_run_make(
        dir  => $LEMON_DIR,
        args => [],
    );
}

# Run all .y files through lemon.
sub ACTION_parsers {
    my $self = shift;
    $self->depends_on('lemon');
    my $y_files = $self->rscan_dir( $CFC_SOURCE_DIR, qr/\.y$/ );
    for my $y_file (@$y_files) {
        my $c_file = $y_file;
        my $h_file = $y_file;
        $c_file =~ s/\.y$/.c/ or die "no match";
        $h_file =~ s/\.y$/.h/ or die "no match";
        next if $self->up_to_date( $y_file, $c_file );
        $self->add_to_cleanup( $c_file, $h_file );
        my $lemon_report_file = $y_file;
        $lemon_report_file =~ s/\.y$/.out/ or die "no match";
        $self->add_to_cleanup($lemon_report_file);
        system( $LEMON_EXE_PATH, '-c', $y_file ) and die "lemon failed";
    }
}

# Run all .l files through flex.
sub ACTION_lexers {
    my $self = shift;
    my $l_files = $self->rscan_dir( $CFC_SOURCE_DIR, qr/\.l$/ );
    # Rerun flex if lemon file changes.
    my $y_files = $self->rscan_dir( $CFC_SOURCE_DIR, qr/\.y$/ );
    for my $l_file (@$l_files) {
        my $c_file = $l_file;
        my $h_file = $l_file;
        $c_file =~ s/\.l$/.c/ or die "no match";
        $h_file =~ s/\.l$/.h/ or die "no match";
        next
            if $self->up_to_date( [ $l_file, @$y_files ],
            [ $c_file, $h_file ] );
        system( 'flex', '--nounistd', '-o', $c_file, "--header-file=$h_file", $l_file )
            and die "flex failed";
    }
}

sub ACTION_code {
    my $self = shift;

    $self->depends_on(qw( charmony ppport parsers ));

    my @flags = $self->split_like_shell($self->charmony("EXTRA_CFLAGS"));
    # The flag for the MSVC6 hack contains spaces. Make sure it stays quoted.
    @flags = map { /\s/ ? qq{"$_"} : $_ } @flags;

    $self->extra_compiler_flags( '-DCFCPERL', @flags );

    $self->SUPER::ACTION_code;
}

1;

