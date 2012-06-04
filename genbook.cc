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

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>

#include "block.h"
#include "ddrescue.h"


// Return values: 0 OK, -1 interrupted, -2 logfile error.
//
int Genbook::check_all()
  {
  long long pos = ( ( offset() >= 0 ) ? 0 : -offset() );
  if( current_status() == generating && domain().includes( current_pos() ) &&
      ( offset() >= 0 || current_pos() >= -offset() ) )
    pos = current_pos();
  bool first_post = true;

  while( pos >= 0 )
    {
    Block b( pos, softbs() );
    find_chunk( b, Sblock::non_tried );
    if( b.size() <= 0 ) break;
    pos = b.end();
    current_status( generating );
    current_pos( b.pos() );
    if( verbosity >= 0 )
      { show_status( b.pos(), "Generating logfile...", first_post );
        first_post = false; }
    if( interrupted() ) return -1;
    int copied_size = 0, error_size = 0;
    check_block( b, copied_size, error_size );
    if( copied_size + error_size < b.size() )		// EOF
      truncate_vector( b.pos() + copied_size + error_size );
    if( !update_logfile() ) return -2;
    }
  return 0;
  }


// Return values: 1 write error, 0 OK.
//
int Genbook::do_generate( const int odes )
  {
  recsize = 0; gensize = 0;
  odes_ = odes;

  for( int i = 0; i < sblocks(); ++i )
    {
    const Sblock & sb = sblock( i );
    if( !domain().includes( sb ) )
      { if( domain() < sb ) break; else continue; }
    if( sb.status() == Sblock::finished ) recsize += sb.size();
    if( sb.status() != Sblock::non_tried || i + 1 < sblocks() )
      gensize += sb.size();
    }
  set_signals();
  if( verbosity >= 0 )
    {
    std::printf( "Press Ctrl-C to interrupt\n" );
    if( logfile_exists() )
      {
      std::printf( "Initial status (read from logfile)\n" );
      std::printf( "rescued: %10sB,", format_num( recsize ) );
      std::printf( "  generated:%10sB\n", format_num( gensize ) );
      std::printf( "Current status\n" );
      }
    }
  int retval = check_all();
  if( verbosity >= 0 )
    {
    show_status( -1, (retval ? 0 : "Finished"), true );
    if( retval == -2 ) std::printf( "Logfile error" );
    else if( retval < 0 ) std::printf( "\nInterrupted by user" );
    std::fputc( '\n', stdout );
    }
  if( retval == -2 ) retval = 1;		// logfile error
  else
    {
    if( retval == 0 ) current_status( finished );
    else if( retval < 0 ) retval = 0;		// interrupted by user
    compact_sblock_vector();
    if( !update_logfile( -1, true ) && retval == 0 ) retval = 1;
    }
  if( final_msg() ) show_error( final_msg(), final_errno() );
  return retval;
  }
