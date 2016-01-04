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

class Sliding_average		// Calculates the average of the last N terms
  {
  unsigned index;
  std::vector<long long> data;

public:
  Sliding_average( const unsigned terms ) : index( terms )
    { data.reserve( terms ); }

  void reset() { if( index < data.size() ) index = data.size(); data.clear(); }

  void add_term( const long long term )
    {
    if( index < data.size() ) data[index++] = term;
    else if( index > data.size() ) data.push_back( term );
    if( index == data.size() ) index = 0;
    }

  long long operator()() const
    {
    long long avg = 0;
    for( unsigned i = 0; i < data.size(); ++i ) avg += data[i];
    if( data.size() ) avg /= data.size();
    return avg;
    }
  };


struct Rb_options
  {
  enum { default_skipbs = 65536, max_max_skipbs = 1 << 30 };

  long long max_error_rate;
  long long min_outfile_size;
  long long max_read_rate;
  long long min_read_rate;
  long max_errors;
  long pause;
  long timeout;
  int cpass_bitset;		// 1 | 2 | 4 for passes 1, 2, 3
  int max_retries;
  int o_direct_in;		// O_DIRECT or 0
  int o_direct_out;		// O_DIRECT or 0
  int preview_lines;		// preview lines to show. 0 = disable
  int skipbs;			// initial size to skip on read error
  int max_skipbs;		// maximum size to skip on read error
  bool complete_only;
  bool exit_on_error;
  bool new_errors_only;
  bool noscrape;
  bool notrim;
  bool reopen_on_error;
  bool retrim;
  bool reverse;
  bool sparse;
  bool try_again;
  bool unidirectional;
  bool verify_on_error;

  Rb_options()
    : max_error_rate( -1 ), min_outfile_size( -1 ), max_read_rate( 0 ),
      min_read_rate( -1 ), max_errors( -1 ), pause( 0 ), timeout( -1 ),
      cpass_bitset( 7 ), max_retries( 0 ), o_direct_in( 0 ), o_direct_out( 0 ),
      preview_lines( 0 ), skipbs( default_skipbs ), max_skipbs( max_max_skipbs ),
      complete_only( false ), exit_on_error( false ),
      new_errors_only( false ), noscrape( false ), notrim( false ),
      reopen_on_error( false ), retrim( false ), reverse( false ),
      sparse( false ), try_again( false ), unidirectional( false ),
      verify_on_error( false )
      {}

  bool operator==( const Rb_options & o ) const
    { return ( max_error_rate == o.max_error_rate &&
               min_outfile_size == o.min_outfile_size &&
               max_read_rate == o.max_read_rate &&
               min_read_rate == o.min_read_rate &&
               max_errors == o.max_errors && pause == o.pause &&
               timeout == o.timeout && cpass_bitset == o.cpass_bitset &&
               max_retries == o.max_retries &&
               o_direct_in == o.o_direct_in && o_direct_out == o.o_direct_out &&
               preview_lines == o.preview_lines &&
               skipbs == o.skipbs && max_skipbs == o.max_skipbs &&
               complete_only == o.complete_only &&
               exit_on_error == o.exit_on_error &&
               new_errors_only == o.new_errors_only &&
               noscrape == o.noscrape && notrim == o.notrim &&
               reopen_on_error == o.reopen_on_error &&
               retrim == o.retrim && reverse == o.reverse &&
               sparse == o.sparse && try_again == o.try_again &&
               unidirectional == o.unidirectional &&
               verify_on_error == o.verify_on_error ); }
  bool operator!=( const Rb_options & o ) const
    { return !( *this == o ); }
  };


class Rescuebook : public Mapbook, public Rb_options
  {
  long long error_rate;
  long long sparse_size;		// end position of pending writes
  long long non_tried_size, non_trimmed_size, non_scraped_size;
  long long bad_sector_size, finished_size;
  const Domain * const test_domain;	// good/bad map for test mode
  const char * const iname_;
  int e_code;				// error code for errors_or_timeout
					// 1 rate, 2 errors, 4 timeout
  long errors;				// error areas found so far
  int ides_, odes_;			// input and output file descriptors
  const bool synchronous_;
  long long voe_ipos;			// pos of last good sector read, or -1
  uint8_t * const voe_buf;		// copy of last good sector read
					// variables for update_rates
  long long a_rate, c_rate, first_size, last_size;
  long long iobuf_ipos;			// last pos read in iobuf, or -1
  long long last_ipos;
  long t0, t1, ts;			// start, current, last successful
  int oldlen;
  bool rates_updated;
  Sliding_average sliding_avg;		// variables for show_status
  bool first_post;			// first read in current pass
  bool first_read;			// first read overall

  void change_chunk_status( const Block & b, const Sblock::Status st );
  bool extend_outfile_size();
  int copy_block( const Block & b, int & copied_size, int & error_size );
  void initialize_sizes();
  bool errors_or_timeout()
    { if( max_errors >= 0 && errors > max_errors ) e_code |= 2;
      return ( e_code != 0 ); }
  void reduce_min_read_rate()
    { if( min_read_rate > 0 ) min_read_rate /= 10; }
  bool slow_read() const
    { return ( t1 - t0 >= 30 &&		// no slow reads for first 30s
               ( ( min_read_rate > 0 && c_rate < min_read_rate &&
                   c_rate < a_rate / 2 ) ||
                 ( min_read_rate == 0 && c_rate < a_rate / 10 ) ) ); }
  int copy_and_update( const Block & b, int & copied_size,
                       int & error_size, const char * const msg,
                       const Status curr_st, const bool forward,
                       const Sblock::Status st = Sblock::bad_sector );
  bool reopen_infile();
  int copy_non_tried();
  int fcopy_non_tried( const char * const msg, const int pass );
  int rcopy_non_tried( const char * const msg, const int pass );
  int trim_errors();
  int scrape_errors();
  int copy_errors();
  int fcopy_errors( const char * const msg, const int retry );
  int rcopy_errors( const char * const msg, const int retry );
  void update_rates( const bool force = false );
  void show_status( const long long ipos, const char * const msg = 0,
                    const bool force = false );
public:
  Rescuebook( const long long offset, const long long isize,
              Domain & dom, const Domain * const test_dom,
              const Rb_options & rb_opts, const char * const iname,
              const char * const mapname, const int cluster,
              const int hardbs, const bool synchronous );

  int do_rescue( const int ides, const int odes );
  };
