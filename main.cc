/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012,
    2013 Antonio Diaz Diaz.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
    Return values: 0 for a normal exit, 1 for environmental problems
    (file not found, invalid flags, I/O errors, etc), 2 to indicate a
    corrupt or invalid input file, 3 for an internal consistency error
    (eg, bug) which caused ddrescue to panic.
*/

#define _FILE_OFFSET_BITS 64

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "arg_parser.h"
#include "rational.h"
#include "block.h"
#include "ddrescue.h"

#ifndef O_DIRECT
#define O_DIRECT 0
#endif


namespace {

const char * const Program_name = "GNU ddrescue";
const char * const program_name = "ddrescue";
const char * invocation_name = 0;

enum Mode { m_none, m_fill, m_generate };
const mode_t outmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

#ifdef O_BINARY
const int o_binary = O_BINARY;
#else
const int o_binary = 0;
#endif


void show_help( const int cluster, const int hardbs, const int skipbs )
  {
  std::printf( "%s - Data recovery tool.\n", Program_name );
  std::printf( "Copies data from one file or block device to another,\n"
               "trying hard to rescue data in case of read errors.\n"
               "\nUsage: %s [options] infile outfile [logfile]\n", invocation_name );
  std::printf( "You should use a logfile unless you know what you are doing.\n"
               "If you reboot, check the device names before restarting ddrescue.\n"
               "\nOptions:\n"
               "  -h, --help                     display this help and exit\n"
               "  -V, --version                  output version information and exit\n"
               "  -a, --min-read-rate=<bytes>    minimum read rate of good areas in bytes/s\n"
               "  -A, --try-again                mark non-split, non-trimmed blocks as non-tried\n"
               "  -b, --sector-size=<bytes>      sector size of input device [default %d]\n", hardbs );
  std::printf( "  -B, --binary-prefixes          show binary multipliers in numbers [SI]\n"
               "  -c, --cluster-size=<sectors>   sectors to copy at a time [%d]\n", cluster );
  std::printf( "  -C, --complete-only            do not read new data beyond logfile limits\n"
               "  -d, --direct                   use direct disc access for input file\n"
               "  -D, --synchronous              use synchronous writes for output file\n"
               "  -e, --max-errors=[+]<n>        maximum number of [new] error areas allowed\n"
               "  -E, --max-error-rate=<bytes>   maximum allowed rate of read errors per second\n"
               "  -f, --force                    overwrite output device or partition\n"
               "  -F, --fill=<types>             fill given type blocks with infile data (?*/-+)\n"
               "  -g, --generate-logfile         generate approximate logfile from partial copy\n"
               "  -i, --input-position=<bytes>   starting position in input file [0]\n"
               "  -I, --verify-input-size        verify input file size with size in logfile\n"
               "  -K, --skip-size=<bytes>        initial size to skip on read error [%sB]\n",
               format_num( skipbs, 9999, -1 ) );
  std::printf( "  -l, --logfile-size=<entries>   do not grow logfile beyond this size [1000]\n"
               "  -m, --domain-logfile=<file>    restrict domain to finished blocks in file\n"
               "  -M, --retrim                   mark all failed blocks as non-trimmed\n"
               "  -n, --no-split                 do not try to split or retry failed blocks\n"
               "  -o, --output-position=<bytes>  starting position in output file [ipos]\n"
               "  -p, --preallocate              preallocate space on disc for output file\n"
               "  -q, --quiet                    suppress all messages\n"
               "  -r, --retries=<n>              exit after given retries (-1=infinity) [0]\n"
               "  -R, --reverse                  reverse direction of copy operations\n"
               "  -s, --size=<bytes>             maximum size of input data to be copied\n"
               "  -S, --sparse                   use sparse writes for output file\n"
               "  -t, --truncate                 truncate output file to zero size\n"
               "  -T, --timeout=<interval>       maximum time since last successful read\n"
               "  -v, --verbose                  be verbose (a 2nd -v gives more)\n"
               "  -x, --extend-outfile=<bytes>   extend outfile size to be at least this long\n"
               "Numbers may be followed by a multiplier: s = sectors, k = kB = 10^3 = 1000,\n"
               "Ki = KiB = 2^10 = 1024, M = 10^6, Mi = 2^20, G = 10^9, Gi = 2^30, etc...\n"
               "Time intervals have the format 1[.5][smhd] or 1/2[smhd]."
               "\nReport bugs to bug-ddrescue@gnu.org\n"
               "Ddrescue home page: http://www.gnu.org/software/ddrescue/ddrescue.html\n"
               "General help using GNU software: http://www.gnu.org/gethelp\n" );
  }


// Recognized formats: <rational_number>[unit]
// Where the optional "unit" is one of 's', 'm', 'h' or 'd'.
// Returns the number of seconds, or exits with 1 status if error.
//
long parse_time_interval( const char * const s )
  {
  Rational r;
  int c = r.parse( s );

  if( c > 0 )
    {
    switch( s[c] )
      {
      case 'd': r *= 86400; break;			// 24 * 60 * 60
      case 'h': r *= 3600; break;			// 60 * 60
      case 'm': r *= 60; break;
      case 's':
      case  0 : break;
      default : show_error( "Bad unit in time interval", 0, true );
                std::exit( 1 );
      }
    const long interval = r.round();
    if( !r.error() && interval >= 0 ) return interval;
    }
  show_error( "Bad value for time interval.", 0, true );
  std::exit( 1 );
  }


bool check_identical( const char * const iname, const char * const oname,
                      const char * const logname )
  {
  struct stat istat, ostat, logstat;
  bool iexists = false, oexists = false, logexists = false;
  bool same = ( std::strcmp( iname, oname ) == 0 );
  if( !same )
    {
    iexists = ( stat( iname, &istat ) == 0 );
    oexists = ( stat( oname, &ostat ) == 0 );
    if( iexists && oexists && istat.st_ino == ostat.st_ino &&
        istat.st_dev == ostat.st_dev ) same = true;
    }
  if( same )
    { show_error( "Infile and outfile are the same." ); return true; }
  if( logname )
    {
    same = ( std::strcmp( iname, logname ) == 0 );
    if( !same )
      {
      logexists = ( stat( logname, &logstat ) == 0 );
      if( iexists && logexists && istat.st_ino == logstat.st_ino &&
          istat.st_dev == logstat.st_dev ) same = true;
      }
    if( same )
      { show_error( "Infile and logfile are the same." ); return true; }
    if( std::strcmp( oname, logname ) == 0 ||
        ( oexists && logexists && ostat.st_ino == logstat.st_ino &&
          ostat.st_dev == logstat.st_dev ) )
      { show_error( "Outfile and logfile are the same." ); return true; }
    }
  return false;
  }


bool check_files( const char * const iname, const char * const oname,
                  const char * const logname,
                  const long long min_outfile_size, const bool force,
                  const bool generate, const bool preallocate,
                  const bool sparse )
  {
  if( !iname || !oname )
    {
    show_error( "Both input and output files must be specified.", 0, true );
    return false;
    }
  if( check_identical( iname, oname, logname ) ) return false;
  if( !generate && ( min_outfile_size > 0 || !force || preallocate || sparse ) )
    {
    struct stat st;
    if( stat( oname, &st ) == 0 && !S_ISREG( st.st_mode ) )
      {
      show_error( "Output file exists and is not a regular file." );
      if( !force )
        show_error( "Use '--force' if you really want to overwrite it, but be\n"
                    "          aware that all existing data in the output file will be lost.",
                    0, true );
      else if( min_outfile_size > 0 )
        show_error( "Only regular files can be extended.", 0, true );
      else if( preallocate )
        show_error( "Only regular files can be preallocated.", 0, true );
      else if( sparse )
        show_error( "Only regular files can be sparse.", 0, true );
      return false;
      }
    }
  return true;
  }


int do_fill( const long long offset, Domain & domain,
             const char * const iname, const char * const oname,
             const char * const logname,
             const int cluster, const int hardbs,
             const std::string & filltypes, const bool synchronous )
  {
  if( !logname )
    {
    show_error( "Logfile required in fill mode.", 0, true );
    return 1;
    }

  Fillbook fillbook( offset, domain, logname, cluster, hardbs, synchronous );
  if( fillbook.domain().size() == 0 )
    { show_error( "Nothing to do." ); return 0; }

  const int ides = open( iname, O_RDONLY | o_binary );
  if( ides < 0 )
    { show_error( "Can't open input file", errno ); return 1; }
  if( !fillbook.read_buffer( ides ) )
    { show_error( "Error reading fill data from input file." ); return 1; }

  const int odes = open( oname, O_CREAT | O_WRONLY | o_binary, outmode );
  if( odes < 0 )
    { show_error( "Can't open output file", errno ); return 1; }
  if( lseek( odes, 0, SEEK_SET ) )
    { show_error( "Output file is not seekable." ); return 1; }

  if( verbosity >= 0 )
    std::printf( "\n\n%s %s\n", Program_name, PROGVERSION );
  if( verbosity >= 1 )
    {
    std::printf( "About to fill with data from %s blocks of %s marked %s\n",
                 iname, oname, filltypes.c_str() );
    std::printf( "    Maximum size to fill: %sBytes\n",
                 format_num( fillbook.domain().in_size() ) );
    std::printf( "    Starting positions: infile = %sB",
                 format_num( fillbook.domain().pos() ) );
    std::printf( ",  outfile = %sB\n",
                 format_num( fillbook.domain().pos() + fillbook.offset() ) );
    std::printf( "    Copy block size: %3d sectors\n", cluster );
    std::printf( "Sector size: %sBytes\n\n", format_num( hardbs, 99999 ) );
    }

  return fillbook.do_fill( odes, filltypes );
  }


int do_generate( const long long offset, Domain & domain,
                 const char * const iname, const char * const oname,
                 const char * const logname,
                 const int cluster, const int hardbs )
  {
  if( !logname )
    {
    show_error( "Logfile must be specified in generate-logfile mode.", 0, true );
    return 1;
    }

  const int ides = open( iname, O_RDONLY | o_binary );
  if( ides < 0 )
    { show_error( "Can't open input file", errno ); return 1; }
  const long long isize = lseek( ides, 0, SEEK_END );
  if( isize < 0 )
    { show_error( "Input file is not seekable." ); return 1; }

  Genbook genbook( offset, isize, domain, logname, cluster, hardbs );
  if( genbook.domain().size() == 0 )
    { show_error( "Nothing to do." ); return 0; }
  if( !genbook.blank() && genbook.current_status() != Logbook::generating )
    {
    show_error( "Logfile alredy exists and is non-empty.", 0, true );
    return 1;
    }

  const int odes = open( oname, O_RDONLY | o_binary );
  if( odes < 0 )
    { show_error( "Can't open output file", errno ); return 1; }
  if( lseek( odes, 0, SEEK_SET ) )
    { show_error( "Output file is not seekable." ); return 1; }

  if( verbosity >= 0 )
    std::printf( "\n\n%s %s\n", Program_name, PROGVERSION );
  if( verbosity >= 1 )
    {
    std::printf( "About to generate an approximate logfile for %s and %s\n",
                 iname, oname );
    std::printf( "    Starting positions: infile = %sB",
                 format_num( genbook.domain().pos() ) );
    std::printf( ",  outfile = %sB\n",
                 format_num( genbook.domain().pos() + genbook.offset() ) );
    std::printf( "    Copy block size: %3d sectors\n", cluster );
    std::printf( "Sector size: %sBytes\n\n", format_num( hardbs, 99999 ) );
    }
  return genbook.do_generate( odes );
  }


int do_rescue( const long long offset, Domain & domain,
               const Rb_options & rb_opts,
               const char * const iname, const char * const oname,
               const char * const logname, const int cluster,
               const int hardbs, const int o_direct, const int o_trunc,
               const bool preallocate, const bool reverse,
               const bool synchronous, const bool verify_input_size )
  {
  const int ides = open( iname, O_RDONLY | o_direct | o_binary );
  if( ides < 0 )
    { show_error( "Can't open input file", errno ); return 1; }
  const long long isize = lseek( ides, 0, SEEK_END );
  if( isize < 0 )
    { show_error( "Input file is not seekable." ); return 1; }

  Rescuebook rescuebook( offset, isize, domain, rb_opts, iname, logname,
                         cluster, hardbs, synchronous );

  if( verify_input_size )
    {
    if( !rescuebook.logfile_exists() || isize <= 0 ||
        rescuebook.logfile_isize() >= LLONG_MAX )
      {
      show_error( "Can't verify input file size. "
                  "Unfinished logfile or other error." );
      return 1;
      }
    if( rescuebook.logfile_isize() != isize )
      {
      show_error( "Input file size differs from size calculated from logfile." );
      return 1;
      }
    }
  if( rescuebook.domain().size() == 0 )
    {
    if( rb_opts.complete_only )
      { show_error( "Nothing to complete; logfile is missing or empty.", 0, true );
        return 1; }
    show_error( "Nothing to do." ); return 0;
    }
  if( o_trunc && !rescuebook.blank() )
    {
    show_error( "Outfile truncation and logfile input are incompatible.", 0, true );
    return 1;
    }

  const int odes = open( oname, O_CREAT | O_WRONLY | o_trunc | o_binary,
                         outmode );
  if( odes < 0 )
    { show_error( "Can't open output file", errno ); return 1; }
  if( lseek( odes, 0, SEEK_SET ) )
    { show_error( "Output file is not seekable." ); return 1; }
  while( preallocate )
    {
#if defined _POSIX_ADVISORY_INFO && _POSIX_ADVISORY_INFO > 0
    if( posix_fallocate( odes, rescuebook.domain().pos() + rescuebook.offset(),
                         rescuebook.domain().size() ) == 0 ) break;
    if( errno != EINTR )
      { show_error( "Can't preallocate output file", errno ); return 1; }
#else
    show_error( "warning: Preallocation not available." ); break;
#endif
    }

  if( !rescuebook.update_logfile( -1, true ) ) return 1;

  if( verbosity >= 0 )
    std::printf( "\n\n%s %s\n", Program_name, PROGVERSION );
  if( verbosity >= 1 )
    {
    std::printf( "About to copy %sBytes from %s to %s\n",
                 format_num( rescuebook.domain().in_size() ), iname, oname );
    std::printf( "    Starting positions: infile = %sB",
                 format_num( rescuebook.domain().pos() ) );
    std::printf( ",  outfile = %sB\n",
                 format_num( rescuebook.domain().pos() + rescuebook.offset() ) );
    std::printf( "    Copy block size: %3d sectors", cluster );
    std::printf( "       Initial skip size: %d sectors\n",
                 rb_opts.skipbs / hardbs );
    std::printf( "Sector size: %sBytes\n", format_num( hardbs, 99999 ) );
    if( verbosity >= 2 )
      {
      bool nl = false;
      if( rb_opts.max_error_rate >= 0 )
        { nl = true; std::printf( "Max error rate: %8sB/s    ",
                                  format_num( rb_opts.max_error_rate, 99999 ) ); }
      if( rb_opts.max_errors >= 0 )
        {
        nl = true;
        std::printf( "Max %serrors: %d    ",
                     rb_opts.new_errors_only ? "new " : "", rb_opts.max_errors );
        }
      if( rb_opts.max_retries >= 0 )
        { nl = true; std::printf( "Max retries: %d    ", rb_opts.max_retries ); }
      if( nl ) { nl = false; std::printf( "\n" ); }

      if( rb_opts.min_read_rate >= 0 )
        { nl = true; std::printf( "Min read rate:  %8sB/s    ",
                                  format_num( rb_opts.min_read_rate, 99999 ) ); }
      if( rb_opts.timeout >= 0 )
        { nl = true; std::printf( "Max time since last successful read: %s",
                                  format_time( rb_opts.timeout ) ); }
      if( nl ) { nl = false; std::printf( "\n" ); }

      std::printf( "Direct: %s    ", o_direct ? "yes" : "no" );
      std::printf( "Sparse: %s    ", rb_opts.sparse ? "yes" : "no" );
      std::printf( "Split: %s    ", !rb_opts.nosplit ? "yes" : "no" );
      std::printf( "Truncate: %s    ", o_trunc ? "yes" : "no" );
      if( rb_opts.complete_only ) std::printf( "Complete only" );
      std::printf( "\n" );
      if( reverse ) std::printf( "Reverse mode\n" );
      }
    std::printf( "\n" );
    }
  return rescuebook.do_rescue( ides, odes, reverse );
  }

} // end namespace


#include "main_common.cc"


int main( const int argc, const char * const argv[] )
  {
  long long ipos = 0;
  long long opos = -1;
  long long max_size = -1;
  const char * domain_logfile_name = 0;
  const int cluster_bytes = 65536;
  const int default_hardbs = 512;
  const int max_hardbs = Rb_options::max_skipbs;
  int cluster = 0;
  int hardbs = default_hardbs;
  int o_direct = 0;
  int o_trunc = 0;
  Mode program_mode = m_none;
  struct Rb_options rb_opts;
  bool force = false;
  bool preallocate = false;
  bool reverse = false;
  bool synchronous = false;
  bool verify_input_size = false;
  std::string filltypes;
  invocation_name = argv[0];
  command_line = argv[0];
  for( int i = 1; i < argc; ++i )
    { command_line += ' '; command_line += argv[i]; }

  const Arg_parser::Option options[] =
    {
    { 'a', "min-read-rate",     Arg_parser::yes },
    { 'A', "try-again",         Arg_parser::no  },
    { 'b', "block-size",        Arg_parser::yes },
    { 'b', "sector-size",       Arg_parser::yes },
    { 'B', "binary-prefixes",   Arg_parser::no  },
    { 'c', "cluster-size",      Arg_parser::yes },
    { 'C', "complete-only",     Arg_parser::no  },
    { 'd', "direct",            Arg_parser::no  },
    { 'D', "synchronous",       Arg_parser::no  },
    { 'e', "max-errors",        Arg_parser::yes },
    { 'E', "max-error-rate",    Arg_parser::yes },
    { 'f', "force",             Arg_parser::no  },
    { 'F', "fill",              Arg_parser::yes },
    { 'g', "generate-logfile",  Arg_parser::no  },
    { 'h', "help",              Arg_parser::no  },
    { 'i', "input-position",    Arg_parser::yes },
    { 'I', "verify-input-size", Arg_parser::no  },
    { 'K', "skip-size",         Arg_parser::yes },
    { 'l', "logfile-size",      Arg_parser::yes },
    { 'm', "domain-logfile",    Arg_parser::yes },
    { 'M', "retrim",            Arg_parser::no  },
    { 'n', "no-split",          Arg_parser::no  },
    { 'o', "output-position",   Arg_parser::yes },
    { 'p', "preallocate",       Arg_parser::no  },
    { 'q', "quiet",             Arg_parser::no  },
    { 'r', "retries",           Arg_parser::yes },
    { 'r', "max-retries",       Arg_parser::yes },
    { 'R', "reverse",           Arg_parser::no  },
    { 's', "size",              Arg_parser::yes },
    { 's', "max-size",          Arg_parser::yes },
    { 'S', "sparse",            Arg_parser::no  },
    { 't', "truncate",          Arg_parser::no  },
    { 'T', "timeout",           Arg_parser::yes },
    { 'v', "verbose",           Arg_parser::no  },
    { 'V', "version",           Arg_parser::no  },
    { 'x', "extend-outfile",    Arg_parser::yes },
    {  0 , 0,                   Arg_parser::no  } };

  const Arg_parser parser( argc, argv, options );
  if( parser.error().size() )				// bad option
    { show_error( parser.error().c_str(), 0, true ); return 1; }

  int argind = 0;
  for( ; argind < parser.arguments(); ++argind )
    {
    const int code = parser.code( argind );
    if( !code ) break;					// no more options
    const char * const arg = parser.argument( argind ).c_str();
    switch( code )
      {
      case 'a': rb_opts.min_read_rate = getnum( arg, hardbs, 0 ); break;
      case 'A': rb_opts.try_again = true; break;
      case 'b': hardbs = getnum( arg, 0, 1, max_hardbs ); break;
      case 'B': format_num( 0, 0, -1 ); break;		// set binary prefixes
      case 'c': cluster = getnum( arg, 0, 1, INT_MAX ); break;
      case 'C': rb_opts.complete_only = true; break;
      case 'd': o_direct = O_DIRECT;
                if( !o_direct )
                  { show_error( "Direct disc access not available." ); return 1; }
                break;
      case 'D': synchronous = true; break;
      case 'e': rb_opts.new_errors_only = ( *arg == '+' );
                rb_opts.max_errors = getnum( arg, 0, 0, INT_MAX ); break;
      case 'E': rb_opts.max_error_rate = getnum( arg, hardbs, 0 ); break;
      case 'f': force = true; break;
      case 'F': set_mode( program_mode, m_fill ); filltypes = arg;
                check_types( filltypes, "fill" ); break;
      case 'g': set_mode( program_mode, m_generate ); break;
      case 'h': show_help( cluster_bytes / default_hardbs, default_hardbs,
                           Rb_options::default_skipbs );
                return 0;
      case 'i': ipos = getnum( arg, hardbs, 0 ); break;
      case 'I': verify_input_size = true; break;
      case 'K': rb_opts.skipbs = getnum( arg, hardbs, Rb_options::default_skipbs,
                                         Rb_options::max_skipbs ); break;
      case 'l': rb_opts.max_logfile_size = getnum( arg, 0, 1, INT_MAX ); break;
      case 'm': set_name( &domain_logfile_name, arg ); break;
      case 'M': rb_opts.retrim = true; break;
      case 'n': rb_opts.nosplit = true; break;
      case 'o': opos = getnum( arg, hardbs, 0 ); break;
      case 'p': preallocate = true; break;
      case 'q': verbosity = -1; break;
      case 'r': rb_opts.max_retries = getnum( arg, 0, -1, INT_MAX ); break;
      case 'R': reverse = true; break;
      case 's': max_size = getnum( arg, hardbs, -1 ); break;
      case 'S': rb_opts.sparse = true; break;
      case 't': o_trunc = O_TRUNC; break;
      case 'T': rb_opts.timeout = parse_time_interval( arg ); break;
      case 'v': if( verbosity < 4 ) ++verbosity; break;
      case 'V': show_version(); return 0;
      case 'x': rb_opts.min_outfile_size = getnum( arg, hardbs, 1 ); break;
      default : internal_error( "uncaught option" );
      }
    } // end process options

  if( opos < 0 ) opos = ipos;
  if( hardbs < 1 ) hardbs = default_hardbs;
  if( cluster >= INT_MAX / hardbs ) cluster = ( INT_MAX / hardbs ) - 1;
  if( cluster < 1 ) cluster = cluster_bytes / hardbs;
  if( cluster < 1 ) cluster = 1;
  if( rb_opts.skipbs < hardbs )
    rb_opts.skipbs = hardbs;
  else						// make multiple of hardbs
    rb_opts.skipbs = round_up( rb_opts.skipbs, hardbs );

  const char *iname = 0, *oname = 0, *logname = 0;
  if( argind < parser.arguments() ) iname = parser.argument( argind++ ).c_str();
  if( argind < parser.arguments() ) oname = parser.argument( argind++ ).c_str();
  if( argind < parser.arguments() ) logname = parser.argument( argind++ ).c_str();
  if( argind < parser.arguments() )
    { show_error( "Too many files.", 0, true ); return 1; }

  // end scan arguments

  if( !check_files( iname, oname, logname, rb_opts.min_outfile_size, force,
                    program_mode == m_generate, preallocate, rb_opts.sparse ) )
    return 1;

  Domain domain( ipos, max_size, domain_logfile_name );

  switch( program_mode )
    {
    case m_fill:
      if( rb_opts != Rb_options() || o_direct ||
          verify_input_size || preallocate || reverse || o_trunc )
        show_error( "warning: Options -aCdeEIMnOprRStTx are ignored in fill mode." );
      return do_fill( opos - ipos, domain, iname, oname, logname, cluster,
                      hardbs, filltypes, synchronous );
    case m_generate:
      if( rb_opts != Rb_options() || o_direct || synchronous ||
          verify_input_size || preallocate || reverse || o_trunc )
        show_error( "warning: Options -aCdDeEIMnOprRStTx are ignored in generate mode." );
      return do_generate( opos - ipos, domain, iname, oname, logname,
                          cluster, hardbs );
    case m_none:
      return do_rescue( opos - ipos, domain, rb_opts, iname, oname, logname,
                        cluster, hardbs, o_direct, o_trunc, preallocate,
                        reverse, synchronous, verify_input_size );
    }
  }
