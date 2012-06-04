/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
    Antonio Diaz Diaz.

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

namespace {

std::string command_line;


void show_version()
  {
  std::printf( "%s %s\n", Program_name, PROGVERSION );
  std::printf( "Copyright (C) %s Antonio Diaz Diaz.\n", program_year );
  std::printf( "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
               "This is free software: you are free to change and redistribute it.\n"
               "There is NO WARRANTY, to the extent permitted by law.\n" );
  }


long long getnum( const char * const ptr, const int bs,
                  const long long min = LLONG_MIN + 1,
                  const long long max = LLONG_MAX )
  {
  errno = 0;
  char *tail;
  long long result = strtoll( ptr, &tail, 0 );
  if( tail == ptr )
    {
    show_error( "Bad or missing numerical argument.", 0, true );
    std::exit( 1 );
    }

  if( !errno && tail[0] )
    {
    int factor = ( tail[1] == 'i' ) ? 1024 : 1000;
    int exponent = 0;
    bool bad_multiplier = false;
    switch( tail[0] )
      {
      case ' ': break;
      case 'b': if( bs > 0 ) { factor = bs; exponent = 1; }
                else bad_multiplier = true;
                break;
      case 'Y': exponent = 8; break;
      case 'Z': exponent = 7; break;
      case 'E': exponent = 6; break;
      case 'P': exponent = 5; break;
      case 'T': exponent = 4; break;
      case 'G': exponent = 3; break;
      case 'M': exponent = 2; break;
      case 'K': if( factor == 1024 ) exponent = 1; else bad_multiplier = true;
                break;
      case 'k': if( factor == 1000 ) exponent = 1; else bad_multiplier = true;
                break;
      default: bad_multiplier = true;
      }
    if( bad_multiplier )
      {
      show_error( "Bad multiplier in numerical argument.", 0, true );
      std::exit( 1 );
      }
    for( int i = 0; i < exponent; ++i )
      {
      if( LLONG_MAX / factor >= llabs( result ) ) result *= factor;
      else { errno = ERANGE; break; }
      }
    }
  if( !errno && ( result < min || result > max ) ) errno = ERANGE;
  if( errno )
    {
    show_error( "Numerical argument out of limits." );
    std::exit( 1 );
    }
  return result;
  }


void check_types( const std::string & types, const char * const opt_name )
  {
  bool error = false;
  for( unsigned i = 0; i < types.size(); ++i )
    if( !Sblock::isstatus( types[i] ) )
      { error = true; break; }
  if( !types.size() || error )
    {
    char buf[80];
    snprintf( buf, sizeof buf, "Invalid type for '%s' option.", opt_name );
    show_error( buf, 0, true );
    std::exit( 1 );
    }
  }


void set_mode( Mode & program_mode, const Mode new_mode )
  {
  if( program_mode != m_none && program_mode != new_mode )
    {
    show_error( "Only one operation can be specified.", 0, true );
    std::exit( 1 );
    }
  program_mode = new_mode;
  }


void set_name( const char ** domain_logfile_name, const char * new_name )
  {
  if( *domain_logfile_name )
    {
    show_error( "Only one domain logfile can be specified.", 0, true );
    std::exit( 1 );
    }
  *domain_logfile_name = new_name;
  }

} // end namespace


int verbosity = 0;


void show_error( const char * const msg, const int errcode, const bool help )
  {
  if( verbosity >= 0 )
    {
    if( msg && msg[0] )
      {
      std::fprintf( stderr, "%s: %s", program_name, msg );
      if( errcode > 0 )
        std::fprintf( stderr, ": %s", std::strerror( errcode ) );
      std::fprintf( stderr, "\n" );
      }
    if( help )
      std::fprintf( stderr, "Try '%s --help' for more information.\n",
                    invocation_name );
    }
  }


void internal_error( const char * const msg )
  {
  if( verbosity >= 0 )
    std::fprintf( stderr, "%s: internal error: %s.\n", program_name, msg );
  std::exit( 3 );
  }


void write_logfile_header( FILE * const f )
  {
  std::fprintf( f, "# Rescue Logfile. Created by %s version %s\n",
                Program_name, PROGVERSION );
  std::fprintf( f, "# Command line: %s\n", command_line.c_str() );
  }


const char * format_num( long long num, long long limit,
                         const int set_prefix )
  {
  const char * const si_prefix[8] =
    { "k", "M", "G", "T", "P", "E", "Z", "Y" };
  const char * const binary_prefix[8] =
    { "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi" };
  static bool si = true;
  static char buf[16];

  if( set_prefix ) si = ( set_prefix > 0 );
  const int factor = ( si ? 1000 : 1024 );
  const char * const * prefix = ( si ? si_prefix : binary_prefix );
  const char * p = "";
  limit = std::max( 999LL, std::min( 999999LL, limit ) );

  for( int i = 0; i < 8 && llabs( num ) > limit; ++i )
    { num /= factor; p = prefix[i]; }
  snprintf( buf, sizeof buf, "%lld %s", num, p );
  return buf;
  }
