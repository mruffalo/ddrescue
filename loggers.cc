/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2013 Antonio Diaz Diaz.

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

#include <cstdio>
#include <vector>

#include "block.h"
#include "loggers.h"


namespace {

const char * format_time_hms( long t )
  {
  static char buf[16];
  const long s = t % 60;
  const long m = ( t / 60 ) % 60;
  const long h = t / 3600;

  snprintf( buf, sizeof buf, "%2ld:%02ld:%02ld", h, m, s );
  return buf;
  }

} // end namespace


Rate_logger rate_logger;
Read_logger read_logger;


bool Logger::close_file()
  {
  if( f && std::fclose( f ) != 0 ) error = true;
  f = 0;
  return !error;
  }


bool Rate_logger::open_file()
  {
  if( !filename_ ) return true;
  if( !f )
    {
    f = std::fopen( filename_, "w" );
    error = !f || std::fprintf( f, "   Time       Ipos     Current_rate  Average_rate  Errors    Errsize\n" ) < 0;
    }
  return !error;
  }


bool Rate_logger::print_line( const long time, const long long ipos,
                              const long long a_rate, const long long c_rate,
                              const int errors, const long long errsize )
  {
  if( f && !error &&
      std::fprintf( f, "%s  0x%010llX %9sB/s %9sB/s  %7u %9sB\n",
                    format_time_hms( time ), ipos,
                    format_num( c_rate, 99999 ), format_num( a_rate, 99999 ),
                    errors, format_num( errsize, 99999 ) ) < 0 )
    error = true;
  return !error;
  }


bool Read_logger::open_file()
  {
  if( !filename_ ) return true;
  if( !f )
    {
    f = std::fopen( filename_, "w" );
    error = !f || std::fprintf( f, "    Ipos         Size    Copied_size  Error_size\n" ) < 0;
    }
  return !error;
  }


bool Read_logger::print_line( const long long ipos, const long long size,
                              const int copied_size, const int error_size )
  {
  if( f && !error &&
      std::fprintf( f, "0x%010llX %9sB %9sB %9sB\n", ipos,
                    format_num( size, 99999 ), format_num( copied_size, 99999 ),
                    format_num( error_size, 99999 ) ) < 0 )
    error = true;
  return !error;
  }


bool Read_logger::print_msg( const long time, const char * const msg )
  {
  if( f && !error &&
      std::fprintf( f, "Time %s  %s\n", format_time_hms( time ), msg ) < 0 )
    error = true;
  return !error;
  }


bool Read_logger::print_time( const long time )
  {
  if( f && !error && time > 0 &&
      std::fprintf( f, "Time %s\n", format_time_hms( time ) ) < 0 )
    error = true;
  return !error;
  }
