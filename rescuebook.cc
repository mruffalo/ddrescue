/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004-2016 Antonio Diaz Diaz.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
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
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "block.h"
#include "loggers.h"
#include "mapbook.h"
#include "rescuebook.h"


namespace {

// Round "size" to the next multiple of sector size (hardbs).
//
int round_up( int size, const int hardbs )
  {
  if( size % hardbs )
    {
    size -= size % hardbs;
    if( INT_MAX - size >= hardbs ) size += hardbs;
    }
  return size;
  }

} // end namespace


void Rescuebook::change_chunk_status( const Block & b, const Sblock::Status st )
  {
  Sblock::Status old_st = st;
  errors += Mapfile::change_chunk_status( b, st, domain(), &old_st );
  if( st == old_st ) return;
  switch( old_st )
    {
    case Sblock::non_tried:     non_tried_size -= b.size(); break;
    case Sblock::non_trimmed: non_trimmed_size -= b.size(); break;
    case Sblock::non_scraped: non_scraped_size -= b.size(); break;
    case Sblock::bad_sector:   bad_sector_size -= b.size(); break;
    case Sblock::finished:       finished_size -= b.size(); break;
    }
  switch( st )
    {
    case Sblock::non_tried:     non_tried_size += b.size(); break;
    case Sblock::non_trimmed: non_trimmed_size += b.size(); break;
    case Sblock::non_scraped: non_scraped_size += b.size(); break;
    case Sblock::bad_sector:   bad_sector_size += b.size(); break;
    case Sblock::finished:       finished_size += b.size(); break;
    }
  }


bool Rescuebook::extend_outfile_size()
  {
  if( min_outfile_size > 0 || sparse_size > 0 )
    {
    const long long min_size = std::max( min_outfile_size, sparse_size );
    const long long size = lseek( odes_, 0, SEEK_END );
    if( size < 0 ) return false;
    if( min_size > size )
      {
      const uint8_t zero = 0;
      if( writeblock( odes_, &zero, 1, min_size - 1 ) != 1 ) return false;
      fsync( odes_ );
      }
    }
  return true;
  }


// Return values: 2 bad infile, 1 I/O error, 0 OK.
// If OK && copied_size + error_size < b.size(), it means EOF has been reached.
//
int Rescuebook::copy_block( const Block & b, int & copied_size, int & error_size )
  {
  if( b.size() <= 0 ) internal_error( "bad size copying a Block." );
  if( !test_domain || test_domain->includes( b ) )
    {
    if( o_direct_in )
      {
      const int pre = b.pos() % hardbs();
      const int disp = b.end() % hardbs();
      const int post = ( disp > 0 ) ? hardbs() - disp : 0;
      const int size = pre + b.size() + post;
      if( size > iobuf_size() )
        internal_error( "(size > iobuf_size) copying a Block." );
      copied_size = readblock( ides_, iobuf(), size, b.pos() - pre );
      copied_size -= std::min( pre, copied_size );
      if( copied_size > b.size() ) copied_size = b.size();
      if( pre > 0 && copied_size > 0 )
        std::memmove( iobuf(), iobuf() + pre, copied_size );
      }
    else copied_size = readblock( ides_, iobuf(), b.size(), b.pos() );
    error_size = errno ? b.size() - copied_size : 0;
    if( errno == EINVAL )
      { final_msg( "Unaligned read error. Is sector size correct?" ); return 1; }
    }
  else { copied_size = 0; error_size = b.size(); }

  if( copied_size > 0 )
    {
    iobuf_ipos = b.pos();
    const long long pos = b.pos() + offset();
    if( sparse_size >= 0 && block_is_zero( iobuf(), copied_size ) )
      {
      const long long end = pos + copied_size;
      if( end > sparse_size ) sparse_size = end;
      }
    else if( writeblock( odes_, iobuf(), copied_size, pos ) != copied_size ||
             ( synchronous_ && fsync( odes_ ) < 0 && errno != EINVAL ) )
      { final_msg( "Write error", errno ); return 1; }
    }
  else iobuf_ipos = -1;

  read_logger.print_line( b.pos(), b.size(), copied_size, error_size );

  if( verify_on_error )
    {
    if( copied_size >= hardbs() && b.pos() % hardbs() == 0 )
      { voe_ipos = b.pos(); std::memcpy( voe_buf, iobuf(), hardbs() ); }
    else if( copied_size <= 0 && error_size > 0 && voe_ipos >= 0 )
      {
      const int size = readblock( ides_, iobuf(), hardbs(), voe_ipos );
      if( size != hardbs() )
        { final_msg( "Input file no longer returns data", errno ); return 2; }
      if( std::memcmp( voe_buf, iobuf(), hardbs() ) != 0 )
        { final_msg( "Input file returns inconsistent data" ); return 2; }
      }
    }
  return 0;
  }


void Rescuebook::initialize_sizes()
  {
  bool good = true;
  non_tried_size = non_trimmed_size = non_scraped_size = 0;
  bad_sector_size = finished_size = 0;
  errors = 0;

  for( long i = 0; i < sblocks(); ++i )
    {
    const Sblock & sb = sblock( i );
    if( !domain().includes( sb ) )
      { if( domain() < sb ) break; else { good = true; continue; } }
    switch( sb.status() )
      {
      case Sblock::non_tried:     non_tried_size += sb.size(); good = true; break;
      case Sblock::non_trimmed: non_trimmed_size += sb.size(); good = true; break;
      case Sblock::non_scraped: non_scraped_size += sb.size(); good = true; break;
      case Sblock::bad_sector:   bad_sector_size += sb.size();
        if( good ) { good = false; ++errors; } break;
      case Sblock::finished:       finished_size += sb.size(); good = true; break;
      }
    }
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted.
//
int Rescuebook::copy_and_update( const Block & b, int & copied_size,
                                 int & error_size, const char * const msg,
                                 const Status curr_st, const bool forward,
                                 const Sblock::Status st )
  {
  if( first_post )
    {
    if( first_read ) first_read = false;
    else if( pause > 0 )
      {
      show_status( -1, "Paused", true );
      sleep( pause );
      const long t2 = std::time( 0 );
      if( t1 < t2 ) t1 = t2;			// clock may have jumped back
      ts = std::min( ts + pause, t2 );		// avoid spurious timeout
      }
    current_status( curr_st, msg );
    read_logger.print_msg( t1 - t0, msg );
    }
  current_pos( forward ? b.pos() : b.end() );
  show_status( b.pos(), msg );
  if( errors_or_timeout() ) return 1;
  if( interrupted() ) return -1;
  int retval = copy_block( b, copied_size, error_size );
  if( retval == 0 )
    {
    if( copied_size + error_size < b.size() )			// EOF
      {
      if( complete_only ) truncate_domain( b.pos() + copied_size + error_size );
      else if( !truncate_vector( b.pos() + copied_size + error_size ) )
        { final_msg( "EOF found below the size calculated from mapfile" );
          retval = 1; }
      initialize_sizes();
      }
    if( copied_size > 0 )
      change_chunk_status( Block( b.pos(), copied_size ), Sblock::finished );
    if( error_size > 0 )
      {
      error_rate += error_size;
      const Sblock::Status st2 =
        ( error_size > hardbs() ) ? st : Sblock::bad_sector;
      change_chunk_status( Block( b.pos() + copied_size, error_size ), st2 );
      struct stat istat;
      if( stat( iname_, &istat ) != 0 )
        { final_msg( "Input file disappeared", errno ); retval = 1; }
      }
    }
  return retval;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 mapfile error.
// Read the non-tried part of the domain, skipping over the damaged areas.
//
int Rescuebook::copy_non_tried()
  {
  char msgbuf[80] = "Copying non-tried blocks... Pass ";
  const int msglen = std::strlen( msgbuf );
  bool forward = !reverse;

  for( int pass = 1; pass <= 3; ++pass )
    {
    if( cpass_bitset & ( 1 << ( pass - 1 ) ) )
      {
      first_post = true;
      snprintf( msgbuf + msglen, ( sizeof msgbuf ) - msglen, "%d %s",
                pass, forward ? "(forwards)" : "(backwards)" );
      int retval = forward ? fcopy_non_tried( msgbuf, pass ) :
                             rcopy_non_tried( msgbuf, pass );
      if( retval != -3 ) return retval;
      reduce_min_read_rate();
      }
    if( !unidirectional ) forward = !forward;
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 mapfile error.
// Read forwards the non-tried part of the domain, skipping over the
// damaged areas.
//
int Rescuebook::fcopy_non_tried( const char * const msg, const int pass )
  {
  long long pos = 0;
  int skip_size = skipbs;		// size to skip on error if skipbs > 0
  bool block_found = false;

  if( pass == 1 && current_status() == copying &&
      domain().includes( current_pos() ) )
    {
    Block b( current_pos(), 1 );
    find_chunk( b, Sblock::non_tried, domain(), hardbs() );
    if( b.size() > 0 ) pos = b.pos();		// resume
    }

  while( pos >= 0 )
    {
    Block b( pos, softbs() );
    find_chunk( b, Sblock::non_tried, domain(), softbs() );
    if( b.size() <= 0 ) break;
    if( pos != b.pos() ) skip_size = skipbs;	// reset size on block change
    pos = b.end();
    block_found = true;
    int copied_size = 0, error_size = 0;
    const int retval = copy_and_update( b, copied_size, error_size, msg,
                                        copying, true, Sblock::non_trimmed );
    if( retval ) return retval;
    update_rates();
    if( error_size > 0 && exit_on_error ) { e_code |= 2; return 1; }
    if( ( error_size > 0 || slow_read() ) && pos >= 0 )
      {
      if( reopen_on_error && !reopen_infile() ) return 1;
      if( skipbs > 0 && pass <= 2 )		// do not skip if skipbs == 0
        {
        b.assign( pos, skip_size );
        find_chunk( b, Sblock::non_tried, domain(), hardbs() );
        if( pos == b.pos() && b.size() > 0 ) pos = b.end();	// skip
        if( skip_size <= max_skipbs / 2 ) skip_size *= 2;
        else skip_size = max_skipbs;
        }
      }
    else if( copied_size > 0 ) skip_size = skipbs;		// reset
    if( !update_mapfile( odes_ ) ) return -2;
    }
  if( !block_found ) return 0;
  return -3;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 mapfile error.
// Read backwards the non-tried part of the domain, skipping over the
// damaged areas.
//
int Rescuebook::rcopy_non_tried( const char * const msg, const int pass )
  {
  long long end = LLONG_MAX;
  int skip_size = skipbs;		// size to skip on error if skipbs > 0
  bool block_found = false;

  if( pass == 1 && current_status() == copying &&
      domain().includes( current_pos() - 1 ) )
    {
    Block b( current_pos() - 1, 1 );
    rfind_chunk( b, Sblock::non_tried, domain(), hardbs() );
    if( b.size() > 0 ) end = b.end();		// resume
    }

  while( end > 0 )
    {
    Block b( end - softbs(), softbs() );
    rfind_chunk( b, Sblock::non_tried, domain(), softbs() );
    if( b.size() <= 0 ) break;
    if( end != b.end() ) skip_size = skipbs;	// reset size on block change
    end = b.pos();
    block_found = true;
    int copied_size = 0, error_size = 0;
    const int retval = copy_and_update( b, copied_size, error_size, msg,
                                        copying, false, Sblock::non_trimmed );
    if( retval ) return retval;
    update_rates();
    if( error_size > 0 && exit_on_error ) { e_code |= 2; return 1; }
    if( ( error_size > 0 || slow_read() ) && end > 0 )
      {
      if( reopen_on_error && !reopen_infile() ) return 1;
      if( skipbs > 0 && pass <= 2 )		// do not skip if skipbs == 0
        {
        b.assign( end - skip_size, skip_size );
        rfind_chunk( b, Sblock::non_tried, domain(), hardbs() );
        if( end == b.end() && b.size() > 0 ) end = b.pos();	// skip
        if( skip_size <= max_skipbs / 2 ) skip_size *= 2;
        else skip_size = max_skipbs;
        }
      }
    else if( copied_size > 0 ) skip_size = skipbs;		// reset
    if( !update_mapfile( odes_ ) ) return -2;
    }
  if( !block_found ) return 0;
  return -3;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 mapfile error.
// Trim both edges of each damaged area sequentially.
//
int Rescuebook::trim_errors()
  {
  const char * const msg = reverse ? "Trimming failed blocks... (backwards)" :
                                     "Trimming failed blocks... (forwards)";
  first_post = true;

  for( long i = 0; i < sblocks(); )
    {
    const Sblock sb = sblock( reverse ? sblocks() - i - 1 : i );
    if( !domain().includes( sb ) )
      { if( ( !reverse && domain() < sb ) || ( reverse && domain() > sb ) )
          break;
        ++i; continue; }
    if( sb.status() != Sblock::non_trimmed ) { ++i; continue; }
    long long pos = sb.pos();
    long long end = sb.end();
    bool error_found = false;
    while( pos < end && !error_found )
      {
      Block b( pos, std::min( (long long)hardbs(), end - pos ) );
      if( b.end() != end ) b.align_end( hardbs() );
      pos = b.end();
      int copied_size = 0, error_size = 0;
      const int retval = copy_and_update( b, copied_size, error_size, msg,
                                          trimming, true );
      if( retval ) return retval;
      if( error_size > 0 ) error_found = true;
      update_rates();
      if( !update_mapfile( odes_ ) ) return -2;
      }
    error_found = false;
    while( end > pos && !error_found )
      {
      const int size = std::min( (long long)hardbs(), end - pos );
      Block b( end - size, size );
      if( b.pos() != pos ) b.align_pos( hardbs() );
      end = b.pos();
      int copied_size = 0, error_size = 0;
      const int retval = copy_and_update( b, copied_size, error_size, msg,
                                          trimming, false );
      if( retval ) return retval;
      if( error_size > 0 ) error_found = true;
      if( error_size > 0 && end > pos )
        {
        const long index = find_index( end - 1 );
        if( index >= 0 && domain().includes( sblock( index ) ) &&
            sblock( index ).status() == Sblock::non_trimmed )
          change_chunk_status( sblock( index ), Sblock::non_scraped );
        }
      update_rates();
      if( !update_mapfile( odes_ ) ) return -2;
      }
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 mapfile error.
// Scrape the damaged areas sequentially.
//
int Rescuebook::scrape_errors()
  {
  const char * const msg = reverse ? "Scraping failed blocks... (backwards)" :
                                     "Scraping failed blocks... (forwards)";
  first_post = true;

  for( long i = 0; i < sblocks(); )
    {
    const Sblock sb = sblock( reverse ? sblocks() - i - 1 : i );
    if( !domain().includes( sb ) )
      { if( ( !reverse && domain() < sb ) || ( reverse && domain() > sb ) )
          break;
        ++i; continue; }
    if( sb.status() != Sblock::non_scraped ) { ++i; continue; }
    long long pos = sb.pos();
    const long long end = sb.end();
    while( pos < end )
      {
      Block b( pos, std::min( (long long)hardbs(), end - pos ) );
      if( b.end() != end ) b.align_end( hardbs() );
      pos = b.end();
      int copied_size = 0, error_size = 0;
      const int retval = copy_and_update( b, copied_size, error_size, msg,
                                          scraping, true );
      if( retval ) return retval;
      update_rates();
      if( !update_mapfile( odes_ ) ) return -2;
      }
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 mapfile error.
// Try to read the damaged areas, one sector at a time.
//
int Rescuebook::copy_errors()
  {
  char msgbuf[80] = "Retrying bad sectors... Retry ";
  const int msglen = std::strlen( msgbuf );
  bool forward = !reverse;

  for( int retry = 1; max_retries < 0 || retry <= max_retries; ++retry )
    {
    first_post = true;
    snprintf( msgbuf + msglen, ( sizeof msgbuf ) - msglen, "%d %s",
              retry, forward ? "(forwards)" : "(backwards)" );
    int retval = forward ? fcopy_errors( msgbuf, retry ) :
                           rcopy_errors( msgbuf, retry );
    if( retval != -3 ) return retval;
    if( !unidirectional ) forward = !forward;
    if( retry >= INT_MAX ) break;
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 mapfile error.
// Try to read forwards the damaged areas, one sector at a time.
//
int Rescuebook::fcopy_errors( const char * const msg, const int retry )
  {
  long long pos = 0;
  bool block_found = false;

  if( retry == 1 && current_status() == retrying &&
      domain().includes( current_pos() ) )
    {
    Block b( current_pos(), 1 );
    find_chunk( b, Sblock::bad_sector, domain(), hardbs() );
    if( b.size() > 0 ) pos = b.pos();		// resume
    }

  while( pos >= 0 )
    {
    Block b( pos, hardbs() );
    find_chunk( b, Sblock::bad_sector, domain(), hardbs() );
    if( b.size() <= 0 ) break;			// no more blocks
    pos = b.end();
    block_found = true;
    int copied_size = 0, error_size = 0;
    const int retval = copy_and_update( b, copied_size, error_size, msg,
                                        retrying, true );
    if( retval ) return retval;
    update_rates();
    if( !update_mapfile( odes_ ) ) return -2;
    }
  if( !block_found ) return 0;
  return -3;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 mapfile error.
// Try to read backwards the damaged areas, one sector at a time.
//
int Rescuebook::rcopy_errors( const char * const msg, const int retry )
  {
  long long end = LLONG_MAX;
  bool block_found = false;

  if( retry == 1 && current_status() == retrying &&
      domain().includes( current_pos() - 1 ) )
    {
    Block b( current_pos() - 1, 1 );
    rfind_chunk( b, Sblock::bad_sector, domain(), hardbs() );
    if( b.size() > 0 ) end = b.end();		// resume
    }

  while( end > 0 )
    {
    Block b( end - hardbs(), hardbs() );
    rfind_chunk( b, Sblock::bad_sector, domain(), hardbs() );
    if( b.size() <= 0 ) break;			// no more blocks
    end = b.pos();
    block_found = true;
    int copied_size = 0, error_size = 0;
    const int retval = copy_and_update( b, copied_size, error_size, msg,
                                        retrying, false );
    if( retval ) return retval;
    update_rates();
    if( !update_mapfile( odes_ ) ) return -2;
    }
  if( !block_found ) return 0;
  return -3;
  }


void Rescuebook::update_rates( const bool force )
  {
  if( t0 == 0 )
    {
    t0 = t1 = ts = initial_time();
    first_size = last_size = finished_size;
    rates_updated = true;
    if( verbosity >= 0 )
      {
      std::fputs( "\n\n\n\n\n", stdout );
      if( preview_lines > 0 )
        for( int i = -2; i < preview_lines; ++i ) std::fputc( '\n', stdout );
      }
    }

  long t2 = std::time( 0 );
  if( max_read_rate > 0 && finished_size - last_size > max_read_rate && t2 == t1 )
    { sleep( 1 ); t2 = std::time( 0 ); }
  if( t2 < t1 )					// clock jumped back
    {
    const long delta = std::min( t0, t1 - t2 );
    t0 -= delta;
    ts -= delta;
    t1 = t2;
    }
  if( force && t2 <= t1 ) t2 = t1 + 1;		// force update of e_code
  if( t2 > t1 )
    {
    a_rate = ( finished_size - first_size ) / ( t2 - t0 );
    c_rate = ( finished_size - last_size ) / ( t2 - t1 );
    if( !( e_code & 4 ) )
      {
      if( finished_size != last_size ) { last_size = finished_size; ts = t2; }
      else if( timeout >= 0 && t2 - ts > timeout && t1 > t0 ) e_code |= 4;
      }
    if( max_error_rate >= 0 && !( e_code & 1 ) )
      {
      error_rate /= ( t2 - t1 );
      if( error_rate > max_error_rate ) e_code |= 1;
      else error_rate = 0;
      }
    t1 = t2;
    rates_updated = true;
    }
  }


void Rescuebook::show_status( const long long ipos, const char * const msg,
                              const bool force )
  {
  const char * const up = "\x1B[A";

  if( ipos >= 0 ) last_ipos = ipos;
  if( rates_updated || force || first_post )
    {
    if( verbosity >= 0 )
      {
      std::printf( "\r%s%s%s%s%s", up, up, up, up, up );
      if( preview_lines > 0 )
        {
        for( int i = -2; i < preview_lines; ++i ) std::fputs( up, stdout );
        std::fputs( "Data preview:\n", stdout );
        for( int i = 0; i < preview_lines; ++i )
          {
          if( iobuf_ipos >= 0 )
            {
            const uint8_t * const p = iobuf() + ( 16 * i );
            std::printf( "%010llX ", ( iobuf_ipos + ( 16 * i ) ) & 0xFFFFFFFFFFLL );
            for( int j = 0; j < 16; ++j )
              { std::printf( " %02X", p[j] );
                if( j == 7 ) std::fputc( ' ', stdout ); }
            std::fputs( "  ", stdout );
            for( int j = 0; j < 16; ++j )
              std::fputc( std::isprint( p[j] ) ? p[j] : '.', stdout );
            std::fputc( '\n', stdout );
            }
          else if( i == ( preview_lines - 1 ) / 2 )
            std::fputs( "                            No data available                                 \n", stdout );
          else
            std::fputs( "                                                                              \n", stdout );
          }
        std::fputc( '\n', stdout );
        }
      std::printf( "  rescued: %9sB, non-trimmed: %9sB,  current rate: %8sB/s\n",
                   format_num( finished_size ), format_num( non_trimmed_size ),
                   format_num( c_rate, 99999 ) );
      std::printf( "non-tried: %9sB, non-scraped: %9sB,  average rate: %8sB/s\n",
                   format_num( non_tried_size ), format_num( non_scraped_size ),
                   format_num( a_rate, 99999 ) );
      std::printf( "     ipos: %9sB,     errsize: %9sB,      run time: %11s\n",
                   format_num( last_ipos ),
                   format_num( bad_sector_size, 99999 ), format_time( t1 - t0 ) );
      if( first_post ) sliding_avg.reset();
      else sliding_avg.add_term( c_rate );
      const long long s_rate = domain().full() ? 0 : sliding_avg();
      const long remaining_time = ( s_rate <= 0 ) ? -1 :
        std::min( std::min( (long long)LONG_MAX, 315359999968464000LL ),
                  ( non_tried_size + non_trimmed_size + non_scraped_size +
                    ( max_retries ? bad_sector_size : 0 ) + s_rate - 1 ) / s_rate );
      std::printf( "     opos: %9sB,      errors: %8ld,  remaining time: %11s\n",
                   format_num( last_ipos + offset() ), errors,
                   format_time( remaining_time, remaining_time >= 180 ) );
      std::printf( "                              time since last successful read: %11s\n",
                   format_time( t1 - ts ) );
      if( msg && msg[0] && !errors_or_timeout() )
        {
        const int len = std::strlen( msg ); std::printf( "\r%s", msg );
        for( int i = len; i < oldlen; ++i ) std::fputc( ' ', stdout );
        oldlen = len;
        }
      std::fflush( stdout );
      }
    rate_logger.print_line( t1 - t0, last_ipos, a_rate, c_rate, errors,
                            bad_sector_size );
    if( !force && !first_post ) read_logger.print_time( t1 - t0 );
    rates_updated = false;
    first_post = false;
    }
  }


Rescuebook::Rescuebook( const long long offset, const long long isize,
                        Domain & dom, const Domain * const test_dom,
                        const Rb_options & rb_opts, const char * const iname,
                        const char * const mapname, const int cluster,
                        const int hardbs, const bool synchronous )
  : Mapbook( offset, isize, dom, mapname, cluster, hardbs, rb_opts.complete_only ),
    Rb_options( rb_opts ),
    error_rate( 0 ),
    sparse_size( sparse ? 0 : -1 ),
    non_tried_size( 0 ),
    non_trimmed_size( 0 ),
    non_scraped_size( 0 ),
    bad_sector_size( 0 ),
    finished_size( 0 ),
    test_domain( test_dom ),
    iname_( iname ),
    e_code( 0 ),
    synchronous_( synchronous ),
    voe_ipos( -1 ), voe_buf( new uint8_t[hardbs] ),
    a_rate( 0 ), c_rate( 0 ), first_size( 0 ), last_size( 0 ),
    iobuf_ipos( -1 ), last_ipos( 0 ), t0( 0 ), t1( 0 ), ts( 0 ), oldlen( 0 ),
    rates_updated( false ), sliding_avg( 30 ), first_post( false ),
    first_read( true )
  {
  if( preview_lines > softbs() / 16 ) preview_lines = softbs() / 16;
  const long long csize = isize / 100;
  if( isize > 0 && skipbs > 0 && max_skipbs == Rb_options::max_max_skipbs &&
      csize < max_skipbs )
    max_skipbs = std::max( (long long)skipbs, csize );
  skipbs = round_up( skipbs, hardbs );		// make multiple of hardbs
  max_skipbs = round_up( max_skipbs, hardbs );

  if( retrim )
    for( long index = 0; index < sblocks(); ++index )
      {
      const Sblock & sb = sblock( index );
      if( !domain().includes( sb ) )
        { if( domain() < sb ) break; else continue; }
      if( sb.status() == Sblock::non_scraped ||
          sb.status() == Sblock::bad_sector )
        change_sblock_status( index, Sblock::non_trimmed );
      }
  if( try_again )
    for( long index = 0; index < sblocks(); ++index )
      {
      const Sblock & sb = sblock( index );
      if( !domain().includes( sb ) )
        { if( domain() < sb ) break; else continue; }
      if( sb.status() == Sblock::non_scraped ||
          sb.status() == Sblock::non_trimmed )
        change_sblock_status( index, Sblock::non_tried );
      }
  initialize_sizes();				// counts errors
  if( new_errors_only ) max_errors += errors;
  }


// Return values: 1 I/O error, 0 OK.
//
int Rescuebook::do_rescue( const int ides, const int odes )
  {
  bool copy_pending = false, trim_pending = false, scrape_pending = false;
  ides_ = ides; odes_ = odes;

  if( non_tried_size ) copy_pending = trim_pending = scrape_pending = true;
  if( non_trimmed_size )              trim_pending = scrape_pending = true;
  if( non_scraped_size )                             scrape_pending = true;
  set_signals();
  if( verbosity >= 0 )
    {
    std::fputs( "Press Ctrl-C to interrupt\n", stdout );
    if( mapfile_exists() )
      {
      std::fputs( "Initial status (read from mapfile)\n", stdout );
      if( verbosity >= 3 )
        {
        std::printf( "current position: %9sB,     current sector: %7lld\n",
                     format_num( current_pos() ), current_pos() / hardbs() );
        if( sblocks() )
          std::printf( " last block size: %9sB\n",
                       format_num( sblock( sblocks() - 1 ).size() ) );
        }
      if( domain().pos() > 0 || domain().end() < mapfile_isize() )
        std::printf( "(sizes below are limited to the domain %sB to %sB)\n",
                     format_num( domain().pos() ), format_num( domain().end() ) );
      std::printf( "  rescued: %9sB,     errsize: %9sB,  errors: %7ld\n",
                   format_num( finished_size ),
                   format_num( bad_sector_size, 99999 ), errors );
      std::fputs( "\nCurrent status\n", stdout );
      }
    }
  int retval = 0;
  update_rates();				// first call
  if( copy_pending && !errors_or_timeout() )
    retval = copy_non_tried();
  if( retval == 0 && trim_pending && !notrim && !errors_or_timeout() )
    retval = trim_errors();
  if( retval == 0 && scrape_pending && !noscrape && !errors_or_timeout() )
    retval = scrape_errors();
  if( retval == 0 && max_retries != 0 && !errors_or_timeout() )
    retval = copy_errors();
  if( !rates_updated ) update_rates( true );	// force update of e_code
  show_status( -1, retval ? 0 : "Finished", true );

  const bool signaled = ( retval == -1 );
  if( signaled ) retval = 0;
  if( retval == 0 && errors_or_timeout() ) retval = 1;
  if( verbosity >= 0 )
    {
    if( retval == -2 ) std::fputs( "\nMapfile error", stdout );
    else if( retval == 0 && signaled )
      std::fputs( "\nInterrupted by user", stdout );
    else
      {
      if( e_code & 1 )
        std::printf( "\nToo high error rate reading input file (%sB/s)",
                     format_num( error_rate ) );
      if( e_code & 2 ) std::fputs( "\nToo many errors in input file", stdout );
      if( e_code & 4 ) std::fputs( "\nTimeout expired", stdout );
      }
    std::fputc( '\n', stdout );
    std::fflush( stdout );
    }
  if( retval == -2 ) retval = 1;		// mapfile error
  else
    {
    if( retval == 0 && !signaled ) current_status( finished );
    if( !extend_outfile_size() )		// sparse or -x option
      {
      show_error( "Error extending output file size." );
      if( retval == 0 ) retval = 1;
      }
    compact_sblock_vector();
    if( !update_mapfile( odes_, true ) && retval == 0 ) retval = 1;
    }
  if( close( odes_ ) != 0 )
    { show_error( "Can't close outfile", errno );
      if( retval == 0 ) retval = 1; }
  if( !rate_logger.close_file() )
    show_error( "warning: Error closing the rates logging file." );
  if( !read_logger.close_file() )
    show_error( "warning: Error closing the reads logging file." );
  if( final_msg().size() ) show_error( final_msg().c_str(), final_errno() );
  if( retval ) return retval;		// errors have priority over signals
  if( signaled ) return signaled_exit();
  return 0;
  }
