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

class Logbook
  {
public:
  enum Status
    { copying = '?', trimming = '*', splitting = '/', retrying = '-',
      filling = 'F', generating = 'G', finished = '+' };

private:
  const long long offset_;		// outfile offset (opos - ipos);
  long long current_pos_;
  long long logfile_isize_;
  Status current_status_;
  Domain & domain_;			// rescue domain
  uint8_t *iobuf_base, *iobuf_;		// iobuf is aligned to page and hardbs
  const char * const filename_;
  const int hardbs_, softbs_;
  const char * final_msg_;
  int final_errno_;
  mutable int index_;			// cached index of last find or change
  std::vector< Sblock > sblock_vector;	// note: blocks are consecutive
  long ul_t1;				// variable for update_logfile
  bool logfile_exists_;

  Logbook( const Logbook & );		// declared as private
  void operator=( const Logbook & );	// declared as private

  void erase_sblock( const int i )
    { sblock_vector.erase( sblock_vector.begin() + i ); }
  void insert_sblock( const int i, const Sblock & sb )
    { sblock_vector.insert( sblock_vector.begin() + i, sb ); }
  void split_domain_border_sblocks();

public:
  Logbook( const long long offset, const long long isize,
           Domain & dom, const char * const logname,
           const int cluster, const int hardbs,
           const bool complete_only, const bool do_not_read = false );
  ~Logbook() { delete[] iobuf_base; }

  bool blank() const;
  void compact_sblock_vector();
  bool update_logfile( const int odes = -1, const bool force = false,
                       const bool retry = true );
  void write_logfile( FILE * const f ) const;

  long long current_pos() const { return current_pos_; }
  Status current_status() const { return current_status_; }
  const Domain & domain() const { return domain_; }
  const char * filename() const { return filename_; }
  uint8_t * iobuf() const { return iobuf_; }
  int hardbs() const { return hardbs_; }
  int softbs() const { return softbs_; }
  long long offset() const { return offset_; }
  const char * final_msg() const { return final_msg_; }
  int final_errno() const { return final_errno_; }
  bool logfile_exists() const { return logfile_exists_; }
  long long logfile_isize() const { return logfile_isize_; }

  void current_pos( const long long pos ) { current_pos_ = pos; }
  void current_status( const Status st ) { current_status_ = st; }
  void final_msg( const char * const msg ) { final_msg_ = msg; }
  void final_errno( const int e ) { final_errno_ = e; }

  const Sblock & sblock( const int i ) const
    { return sblock_vector[i]; }
  int sblocks() const { return (int)sblock_vector.size(); }
  void change_sblock_status( const int i, const Sblock::Status st )
    { sblock_vector[i].status( st ); }
  void split_sblock_by( const long long pos, const int i )
    {
    if( sblock_vector[i].includes( pos ) )
      insert_sblock( i, sblock_vector[i].split( pos ) );
    }
  bool truncate_vector( const long long end, const bool force = false );
  void truncate_domain( const long long end )
    { domain_.crop_by_file_size( end ); }

  int find_index( const long long pos ) const;
  int find_largest_sblock( const Sblock::Status st ) const;
  int find_smallest_sblock( const Sblock::Status st ) const;
  void find_chunk( Block & b, const Sblock::Status st,
                   const int alignment = 0 ) const;
  void rfind_chunk( Block & b, const Sblock::Status st,
                    const int alignment = 0 ) const;
  int change_chunk_status( const Block & b, const Sblock::Status st );

  static bool isstatus( const int st )
    { return ( st == copying || st == trimming || st == splitting ||
               st == retrying || st == filling || st == generating ||
               st == finished ); }
  static const char * status_name( const Status st );
  };


class Fillbook : public Logbook
  {
  long long filled_size;		// size already filled
  long long remaining_size;		// size to be filled
  int filled_areas;			// areas already filled
  int remaining_areas;			// areas to be filled
  int odes_;				// output file descriptor
  const bool ignore_write_errors_;
  const bool synchronous_;
					// variables for show_status
  long long a_rate, c_rate, first_size, last_size;
  long long last_ipos;
  long t0, t1;

  int fill_areas( const std::string & filltypes );
  int fill_block( const Block & b );
  void show_status( const long long ipos, bool force = false );

public:
  Fillbook( const long long offset, Domain & dom,
            const char * const logname, const int cluster, const int hardbs,
            const bool ignore_write_errors, const bool synchronous )
    : Logbook( offset, 0, dom, logname, cluster, hardbs, true ),
      ignore_write_errors_( ignore_write_errors ),
      synchronous_( synchronous ),
      a_rate( 0 ), c_rate( 0 ), first_size( 0 ), last_size( 0 ),
      last_ipos( 0 ), t0( 0 ), t1( 0 )
      {}

  int do_fill( const int odes, const std::string & filltypes );
  bool read_buffer( const int ides );
  };


class Genbook : public Logbook
  {
  long long recsize, gensize;		// total recovered and generated sizes
  int odes_;				// output file descriptor
					// variables for show_status
  long long a_rate, c_rate, first_size, last_size;
  long long last_ipos;
  long t0, t1;
  int oldlen;

  void check_block( const Block & b, int & copied_size, int & error_size );
  int check_all();
  void show_status( const long long ipos, const char * const msg = 0,
                    bool force = false );
public:
  Genbook( const long long offset, const long long isize,
           Domain & dom, const char * const logname,
           const int cluster, const int hardbs )
    : Logbook( offset, isize, dom, logname, cluster, hardbs, false ),
      a_rate( 0 ), c_rate( 0 ), first_size( 0 ), last_size( 0 ),
      last_ipos( 0 ), t0( 0 ), t1( 0 ), oldlen( 0 )
      {}

  int do_generate( const int odes );
  };


struct Rb_options
  {
  enum { default_skipbs = 65536, max_skipbs = 1 << 30 };

  long long max_error_rate;
  long long min_outfile_size;
  long long min_read_rate;
  long timeout;
  int max_errors;
  int max_logfile_size;
  int max_retries;
  int skipbs;			// initial size to skip on read error
  bool complete_only;
  bool new_errors_only;
  bool nosplit;
  bool retrim;
  bool sparse;
  bool try_again;

  Rb_options()
    : max_error_rate( -1 ), min_outfile_size( -1 ), min_read_rate( -1 ),
      timeout( -1 ), max_errors( -1 ), max_logfile_size( 1000 ),
      max_retries( 0 ), skipbs( default_skipbs ), complete_only( false ),
      new_errors_only( false ), nosplit( false ), retrim( false ),
      sparse( false ), try_again( false )
      {}

  bool operator==( const Rb_options & o ) const
    { return ( max_error_rate == o.max_error_rate &&
               min_outfile_size == o.min_outfile_size &&
               min_read_rate == o.min_read_rate && timeout == o.timeout &&
               max_errors == o.max_errors &&
               max_logfile_size == o.max_logfile_size &&
               max_retries == o.max_retries && skipbs == o.skipbs &&
               complete_only == o.complete_only &&
               new_errors_only == o.new_errors_only &&
               nosplit == o.nosplit && retrim == o.retrim &&
               sparse == o.sparse && try_again == o.try_again ); }
  bool operator!=( const Rb_options & o ) const
    { return !( *this == o ); }
  };


class Rescuebook : public Logbook, public Rb_options
  {
  long long error_rate;
  long long sparse_size;		// end position of pending writes
  long long recsize, errsize;		// total recovered and error sizes
  const char * const iname_;
  const int max_skip_size;		// maximum size to skip on read error
  int e_code;				// error code for errors_or_timeout
					// 1 rate, 2 errors, 4 timeout
  int errors;				// error areas found so far
  int ides_, odes_;			// input and output file descriptors
  const bool synchronous_;
					// variables for update_rates
  long long a_rate, c_rate, first_size, last_size;
  long long last_ipos;
  long t0, t1, ts;			// start, current, last successful
  int oldlen;
  bool rates_updated;

  bool extend_outfile_size();
  int copy_block( const Block & b, int & copied_size, int & error_size );
  void count_errors();
  bool errors_or_timeout()
    { if( max_errors >= 0 && errors > max_errors ) e_code |= 2;
      return ( e_code != 0 ); }
  void reduce_min_read_rate()
    { if( min_read_rate > 0 ) min_read_rate /= 10; }
  bool slow_read() const
    { return ( t1 - t0 >= 10 &&		// no slow reads for first 10s
               ( ( min_read_rate > 0 && c_rate < min_read_rate &&
                   c_rate < a_rate / 2 ) ||
                 ( min_read_rate == 0 && c_rate < a_rate / 10 ) ) ); }
  int copy_and_update( const Block & b, const Sblock::Status st,
                       int & copied_size, int & error_size,
                       const char * const msg, bool & first_post,
                       const bool forward );
  int copy_non_tried();
  int rcopy_non_tried();
  int trim_errors();
  int split_errors( const bool reverse );
  int copy_errors();
  int rcopy_errors();
  void update_rates( const bool force = false );
  void show_status( const long long ipos, const char * const msg = 0,
                    const bool force = false );
public:
  Rescuebook( const long long offset, const long long isize,
              Domain & dom, const Rb_options & rb_opts,
              const char * const iname, const char * const logname,
              const int cluster, const int hardbs,
              const bool synchronous );

  int do_rescue( const int ides, const int odes, const bool reverse );
  };


// Round "size" to the next multiple of sector size (hardbs).
//
inline int round_up( int size, const int hardbs )
  {
  if( size % hardbs )
    {
    size -= size % hardbs;
    if( INT_MAX - size >= hardbs ) size += hardbs;
    }
  return size;
  }


// Defined in io.cc
//
const char * format_time( long t );
bool interrupted();
void set_signals();


// Defined in main_common.cc
//
extern int verbosity;
void internal_error( const char * const msg );
void show_error( const char * const msg,
                 const int errcode = 0, const bool help = false );
void write_logfile_header( FILE * const f );
const char * format_num( long long num, long long limit = 999999,
                         const int set_prefix = 0 );
