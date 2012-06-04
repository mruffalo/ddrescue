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

#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>

#include "block.h"
#include "ddrescue.h"


namespace {

int my_fgetc( FILE * const f )
  {
  int ch;
  bool comment = false;

  do {
    ch = std::fgetc( f );
    if( ch == '#' ) comment = true;
    else if( ch == '\n' || ch == EOF ) comment = false;
    }
  while( comment );
  return ch;
  }


// Read a line discarding comments, leading whitespace and blank lines.
// Returns 0 if at EOF.
//
const char * my_fgets( FILE * const f, int & linenum )
  {
  const int maxlen = 127;
  static char buf[maxlen+1];
  int ch, len = 1;

  while( len == 1 )			// while line is blank
    {
    do { ch = my_fgetc( f ); if( ch == '\n' ) ++linenum; }
    while( std::isspace( ch ) );
    len = 0;
    while( true )
      {
      if( ch == EOF ) { if( len > 0 ) ch = '\n'; else break; }
      if( len < maxlen ) buf[len++] = ch;
      if( ch == '\n' ) { ++linenum; break; }
      ch = my_fgetc( f );
      }
    }
  if( len > 0 ) { buf[len] = 0; return buf; }
  else return 0;
  }


void extend_sblock_vector( std::vector< Sblock > & sblock_vector,
                           const long long isize )
  {
  if( sblock_vector.size() == 0 )
    {
    Sblock sb( 0, ( isize > 0 ) ? isize : -1, Sblock::non_tried );
    sb.fix_size();
    sblock_vector.push_back( sb );
    return;
    }
  Sblock & front = sblock_vector.front();
  if( front.pos() > 0 )
    sblock_vector.insert( sblock_vector.begin(), Sblock( 0, front.pos(), Sblock::non_tried ) );
  Sblock & back = sblock_vector.back();
  const long long end = back.end();
  if( isize > 0 )
    {
    if( back.pos() >= isize )
      {
      if( back.pos() == isize && back.status() == Sblock::non_tried )
        { sblock_vector.pop_back(); return; }
      show_error( "Bad logfile; last block begins past end of input file." );
      std::exit( 1 );
      }
    if( end < 0 || end > isize ) back.size( isize - back.pos() );
    else if( end < isize )
      sblock_vector.push_back( Sblock( end, isize - end, Sblock::non_tried ) );
    }
  else if( end >= 0 )
    {
    Sblock sb( end, -1, Sblock::non_tried );
    sb.fix_size();
    if( sb.size() > 0 ) sblock_vector.push_back( sb );
    }
  }


void show_logfile_error( const char * const filename, const int linenum )
  {
  char buf[80];
  snprintf( buf, sizeof buf, "error in logfile %s, line %d", filename, linenum );
  show_error( buf );
  }


// Returns true if logfile exists and is readable.
//
bool read_logfile( const char * const logname,
                   std::vector< Sblock > & sblock_vector,
                   long long & current_pos, Logbook::Status & current_status )
  {
  FILE * const f = std::fopen( logname, "r" );
  if( !f ) return false;
  int linenum = 0;
  sblock_vector.clear();

  const char *line = my_fgets( f, linenum );
  if( line )						// status line
    {
    char ch;
    int n = std::sscanf( line, "%lli %c\n", &current_pos, &ch );
    if( n == 2 && current_pos >= 0 && Logbook::isstatus( ch ) )
      current_status = Logbook::Status( ch );
    else
      {
      show_logfile_error( logname, linenum );
      show_error( "Are you using a logfile from ddrescue 1.5 or older?" );
      std::exit( 2 );
      }

    while( true )
      {
      line = my_fgets( f, linenum );
      if( !line ) break;
      long long pos, size;
      n = std::sscanf( line, "%lli %lli %c\n", &pos, &size, &ch );
      if( n == 3 && pos >= 0 && Sblock::isstatus( ch ) &&
          ( size > 0 || size == -1 || ( size == 0 && pos == 0 ) ) )
        {
        const Sblock::Status st = Sblock::Status( ch );
        Sblock sb( pos, size, st ); sb.fix_size();
        if( sblock_vector.size() > 0 && !sb.follows( sblock_vector.back() ) )
          { show_logfile_error( logname, linenum ); std::exit( 2 ); }
        sblock_vector.push_back( sb );
        }
      else
        { show_logfile_error( logname, linenum ); std::exit( 2 ); }
      }
    }
  std::fclose( f );
  return true;
  }

} // end namespace


Domain::Domain( const long long p, const long long s,
                const char * const logname )
  {
  Block b( p, s ); b.fix_size();
  if( !logname || !logname[0] ) { block_vector.push_back( b ); return; }
  std::vector< Sblock > sblock_vector;
  long long current_pos;			// not used
  Logbook::Status current_status;		// not used
  if( !read_logfile( logname, sblock_vector, current_pos, current_status ) )
    {
    char buf[80];
    snprintf( buf, sizeof buf,
              "Logfile '%s' does not exist or is not readable.", logname );
    show_error( buf );
    std::exit( 1 );
    }
  for( unsigned i = 0; i < sblock_vector.size(); ++i )
    {
    const Sblock & sb = sblock_vector[i];
    if( sb.status() == Sblock::finished ) block_vector.push_back( sb );
    }
  this->crop( b );
  }


void Logbook::split_domain_border_sblocks()
  {
  for( unsigned i = 0; i < sblock_vector.size(); ++i )
    {
    Sblock & sb = sblock_vector[i];
    const long long pos = domain_.breaks_block_by( sb );
    if( pos > 0 )
      {
      const Sblock head( sb.split( pos ) );
      if( head.size() > 0 ) insert_sblock( i, head );
      else internal_error( "empty block created by split_domain_border_sblocks" );
      }
    }
  }


Logbook::Logbook( const long long offset, const long long isize,
                  Domain & dom, const char * const logname,
                  const int cluster, const int hardbs,
                  const bool complete_only, const bool do_not_read )
  : offset_( offset ), current_pos_( 0 ), logfile_isize_( 0 ),
    current_status_( copying ), domain_( dom ), filename_( logname ),
    hardbs_( hardbs ), softbs_( cluster * hardbs ),
    final_msg_( 0 ), final_errno_( 0 ), index_( 0 ),
    ul_t1( std::time( 0 ) )
  {
  int alignment = sysconf( _SC_PAGESIZE );
  if( alignment < hardbs_ || alignment % hardbs_ ) alignment = hardbs_;
  if( alignment < 2 || alignment > 65536 ) alignment = 0;
  iobuf_ = iobuf_base = new uint8_t[ softbs_ + alignment ];
  if( alignment > 1 )		// align iobuf for use with raw devices
    {
    const int disp = alignment - ( reinterpret_cast<long> (iobuf_) % alignment );
    if( disp > 0 && disp < alignment ) iobuf_ += disp;
    }

  if( !domain_.crop_by_file_size( isize ) ) std::exit( 1 );
  if( filename_ && !do_not_read &&
      read_logfile( filename_, sblock_vector, current_pos_, current_status_ ) &&
      sblock_vector.size() )
    logfile_isize_ = sblock_vector.back().end();
  if( !complete_only ) extend_sblock_vector( sblock_vector, isize );
  else if( sblock_vector.size() )  // limit domain to blocks read from logfile
    {
    const Block b( sblock_vector.front().pos(),
                   sblock_vector.back().end() - sblock_vector.front().pos() );
    domain_.crop( b );
    }
  compact_sblock_vector();
  split_domain_border_sblocks();
  if( sblock_vector.size() == 0 ) domain_.clear();
  }


bool Logbook::blank() const
  {
  for( unsigned i = 0; i < sblock_vector.size(); ++i )
    if( sblock_vector[i].status() != Sblock::non_tried )
      return false;
  return true;
  }


void Logbook::compact_sblock_vector()
  {
  for( unsigned i = sblock_vector.size(); i >= 2; )
    {
    --i;
    if( sblock_vector[i-1].join( sblock_vector[i] ) )
      sblock_vector.erase( sblock_vector.begin() + i );
    }
  }


// Writes periodically the logfile to disc.
// Returns false only if update is attempted and fails.
//
bool Logbook::update_logfile( const int odes, const bool force,
                              const bool retry )
  {
  if( !filename_ ) return true;
  const int interval = 30 + std::min( 270, sblocks() / 38 );
  const long t2 = std::time( 0 );
  if( !force && t2 - ul_t1 < interval ) return true;
  ul_t1 = t2;
  if( odes >= 0 ) fsync( odes );

  errno = 0;
  FILE * const f = std::fopen( filename_, "w" );
  if( f )
    {
    write_logfile( f );
    if( std::fclose( f ) == 0 ) return true;
    }

  if( verbosity >= 0 )
    {
    char buf[80];
    const char * const s = ( f ? "Error writing logfile '%s'" :
                                 "Error opening logfile '%s' for writing" );
    snprintf( buf, sizeof buf, s, filename_ );
    if( retry ) std::fprintf( stderr, "\n" );
    show_error( buf, errno );
    if( retry )
      {
      std::fprintf( stderr, "Fix the problem and press ENTER to retry, or Q+ENTER to abort. " );
      std::fflush( stderr );
      while( true )
        {
        const char c = std::tolower( std::fgetc( stdin ) );
        if( c == '\r' || c == '\n' )
          {
          std::fprintf( stderr, "\n\n\n\n" );
          return update_logfile( -1, true );
          }
        if( c == 'q' ) break;
        }
      }
    }
  return false;
  }


void Logbook::write_logfile( FILE * const f ) const
  {
  write_logfile_header( f );
  std::fprintf( f, "# current_pos  current_status\n" );
  std::fprintf( f, "0x%08llX     %c\n", current_pos_, current_status_ );
  std::fprintf( f, "#      pos        size  status\n" );
  for( unsigned i = 0; i < sblock_vector.size(); ++i )
    {
    const Sblock & sb = sblock_vector[i];
    std::fprintf( f, "0x%08llX  0x%08llX  %c\n", sb.pos(), sb.size(), sb.status() );
    }
  }


void Logbook::truncate_vector( const long long pos )
  {
  int i = sblocks() - 1;
  while( i >= 0 && sblock_vector[i].pos() >= pos ) --i;
  if( i < 0 )
    {
    sblock_vector.clear();
    sblock_vector.push_back( Sblock( pos, 0, Sblock::non_tried ) );
    }
  else
    {
    Sblock & sb = sblock_vector[i];
    if( sb.includes( pos ) ) sb.size( pos - sb.pos() );
    sblock_vector.erase( sblock_vector.begin() + i + 1, sblock_vector.end() );
    }
  }


int Logbook::find_index( const long long pos ) const
  {
  if( index_ < 0 || index_ >= sblocks() ) index_ = sblocks() / 2;
  while( index_ + 1 < sblocks() && pos >= sblock_vector[index_].end() )
    ++index_;
  while( index_ > 0 && pos < sblock_vector[index_].pos() )
    --index_;
  if( !sblock_vector[index_].includes( pos ) ) index_ = -1;
  return index_;
  }


// Find chunk from b.pos of size <= b.size and status st.
// if not found, puts b.size to 0.
//
void Logbook::find_chunk( Block & b, const Sblock::Status st,
                          const int alignment ) const
  {
  if( b.size() <= 0 ) return;
  if( b.pos() < sblock_vector.front().pos() )
    b.pos( sblock_vector.front().pos() );
  if( find_index( b.pos() ) < 0 ) { b.size( 0 ); return; }
  int i;
  for( i = index_; i < sblocks(); ++i )
    if( sblock_vector[i].status() == st &&
        domain_.includes( sblock_vector[i] ) )
      { index_ = i; break; }
  if( i >= sblocks() ) { b.size( 0 ); return; }
  if( b.pos() < sblock_vector[index_].pos() )
    b.pos( sblock_vector[index_].pos() );
  b.fix_size();
  if( !sblock_vector[index_].includes( b ) )
    b.crop( sblock_vector[index_] );
  if( b.end() != sblock_vector[index_].end() )
    b.align_end( alignment ? alignment : hardbs_ );
  }


// Find chunk from b.end backwards of size <= b.size and status st.
// if not found, puts b.size to 0.
//
void Logbook::rfind_chunk( Block & b, const Sblock::Status st,
                           const int alignment ) const
  {
  if( b.size() <= 0 ) return;
  b.fix_size();
  if( sblock_vector.back().end() < b.end() )
    b.end( sblock_vector.back().end() );
  find_index( b.end() - 1 );
  for( ; index_ >= 0; --index_ )
    if( sblock_vector[index_].status() == st &&
        domain_.includes( sblock_vector[index_] ) )
      break;
  if( index_ < 0 ) { b.size( 0 ); return; }
  if( b.end() > sblock_vector[index_].end() )
    b.end( sblock_vector[index_].end() );
  if( !sblock_vector[index_].includes( b ) )
    b.crop( sblock_vector[index_] );
  if( b.pos() != sblock_vector[index_].pos() )
    b.align_pos( alignment ? alignment : hardbs_ );
  }


// Returns an adjust value (-1, 0, +1) to keep "errors" updated without
// having to call count_errors every time.
//   - - -   -->   - + -   return +1
//   - - +   -->   - + +   return  0
//   - + -   -->   - - -   return -1
//   - + +   -->   - - +   return  0
//   + - -   -->   + + -   return  0
//   + - +   -->   + + +   return -1
//   + + -   -->   + - -   return  0
//   + + +   -->   + - +   return +1
//
int Logbook::change_chunk_status( const Block & b, const Sblock::Status st )
  {
  if( b.size() <= 0 ) return 0;
  if( !domain_.includes( b ) || find_index( b.pos() ) < 0 ||
      !domain_.includes( sblock_vector[index_] ) )
    internal_error( "can't change status of chunk not in rescue domain" );
  if( !sblock_vector[index_].includes( b ) )
    internal_error( "can't change status of chunk spread over more than 1 block" );
  if( sblock_vector[index_].status() == st ) return 0;

  const bool old_st_good = Sblock::is_good_status( sblock_vector[index_].status() );
  const bool new_st_good = Sblock::is_good_status( st );
  bool bl_st_good = ( index_ <= 0 ||
                      !domain_.includes( sblock_vector[index_-1] ) ||
                      Sblock::is_good_status( sblock_vector[index_-1].status() ) );
  bool br_st_good = ( index_ + 1 >= sblocks() ||
                      !domain_.includes( sblock_vector[index_+1] ) ||
                      Sblock::is_good_status( sblock_vector[index_+1].status() ) );
  if( sblock_vector[index_].pos() < b.pos() )
    {
    if( sblock_vector[index_].end() == b.end() &&
        index_ + 1 < sblocks() && sblock_vector[index_+1].status() == st &&
        domain_.includes( sblock_vector[index_+1] ) )
      {
      sblock_vector[index_].inc_size( -b.size() );
      sblock_vector[index_+1].pos( b.pos() );
      sblock_vector[index_+1].inc_size( b.size() );
      return 0;
      }
    insert_sblock( index_, sblock_vector[index_].split( b.pos() ) );
    ++index_;
    bl_st_good = old_st_good;
    }
  if( sblock_vector[index_].size() > b.size() )
    {
    sblock_vector[index_].pos( b.end() );
    sblock_vector[index_].inc_size( -b.size() );
    br_st_good = Sblock::is_good_status( sblock_vector[index_].status() );
    if( index_ > 0 && sblock_vector[index_-1].status() == st &&
        domain_.includes( sblock_vector[index_-1] ) )
      sblock_vector[index_-1].inc_size( b.size() );
    else
      insert_sblock( index_, Sblock( b, st ) );
    }
  else
    {
    sblock_vector[index_].status( st );
    if( index_ > 0 && sblock_vector[index_-1].status() == st &&
        domain_.includes( sblock_vector[index_-1] ) )
      {
      sblock_vector[index_-1].inc_size( sblock_vector[index_].size() );
      erase_sblock( index_ ); --index_;
      }
    if( index_ + 1 < sblocks() && sblock_vector[index_+1].status() == st &&
        domain_.includes( sblock_vector[index_+1] ) )
      {
      sblock_vector[index_].inc_size( sblock_vector[index_+1].size() );
      erase_sblock( index_ + 1 );
      }
    }
  int retval = 0;
  if( new_st_good != old_st_good && bl_st_good == br_st_good )
    { if( old_st_good == bl_st_good ) retval = +1; else retval = -1; }
  return retval;
  }


const char * Logbook::status_name( const Logbook::Status st )
  {
  switch( st )
    {
    case copying:    return "copying";
    case trimming:   return "trimming";
    case splitting:  return "splitting";
    case retrying:   return "retrying";
    case filling:    return "filling";
    case generating: return "generating";
    case finished:   return "finished";
    }
  return "unknown";			// should not be reached
  }
