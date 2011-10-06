/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011
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

class Logbook
  {
public:
  enum Status
    { copying = '?', trimming = '*', splitting = '/', retrying = '-',
      filling = 'F', generating = 'G', finished = '+' };

private:
  const long long offset_;		// outfile offset (opos - ipos);
  long long current_pos_;
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

  void erase_sblock( const int i )
    { sblock_vector.erase( sblock_vector.begin() + i ); }
  void insert_sblock( const int i, const Sblock & sb )
    { sblock_vector.insert( sblock_vector.begin() + i, sb ); }
  void split_domain_border_sblocks();

public:
  Logbook( const long long ipos, const long long opos, Domain & dom,
           const long long isize, const char * const name,
           const int cluster, const int hardbs, const bool complete_only );
  ~Logbook() { delete[] iobuf_base; }

  bool blank() const throw();
  void compact_sblock_vector();
  bool update_logfile( const int odes = -1, const bool force = false );

  long long current_pos() const throw() { return current_pos_; }
  Status current_status() const throw() { return current_status_; }
  const Domain & domain() const throw() { return domain_; }
  const char *filename() const throw() { return filename_; }
  uint8_t * iobuf() const throw() { return iobuf_; }
  int hardbs() const throw() { return hardbs_; }
  int softbs() const throw() { return softbs_; }
  long long offset() const throw() { return offset_; }
  const char * final_msg() const throw() { return final_msg_; }
  int final_errno() const throw() { return final_errno_; }

  void current_pos( const long long pos ) throw() { current_pos_ = pos; }
  void current_status( Status st ) throw() { current_status_ = st; }
  void final_msg( const char * const msg ) throw() { final_msg_ = msg; }
  void final_errno( const int e ) throw() { final_errno_ = e; }

  const Sblock & sblock( const int i ) const throw()
    { return sblock_vector[i]; }
  int sblocks() const throw() { return (int)sblock_vector.size(); }
  void change_sblock_status( const int i, const Sblock::Status st ) throw()
    { sblock_vector[i].status( st ); }
  void truncate_vector( const long long pos );

  int find_index( const long long pos ) const throw();
  void find_chunk( Block & b, const Sblock::Status st ) const;
  void rfind_chunk( Block & b, const Sblock::Status st ) const;
  int change_chunk_status( const Block & b, const Sblock::Status st );

  static bool isstatus( const int st ) throw()
    { return ( st == copying || st == trimming || st == splitting ||
               st == retrying || st == filling || st == generating ||
               st == finished ); }
  };


class Fillbook : public Logbook
  {
  long long filled_size;		// size already filled
  long long remaining_size;		// size to be filled
  int filled_areas;			// areas already filled
  int remaining_areas;			// areas to be filled
  int odes_;				// output file descriptor
  const bool synchronous_;
					// variables for show_status
  long long a_rate, c_rate, first_size, last_size;
  long long last_ipos;
  long t0, t1;

  int fill_areas( const std::string & filltypes );
  int fill_block( const Block & b );
  void show_status( const long long ipos, bool force = false ) throw();

public:
  Fillbook( const long long ipos, const long long opos, Domain & dom,
            const char * const name, const int cluster, const int hardbs,
            const bool synchronous )
    : Logbook( ipos, opos, dom, 0, name, cluster, hardbs, true ),
      synchronous_( synchronous ),
      a_rate( 0 ), c_rate( 0 ), first_size( 0 ), last_size( 0 ),
      last_ipos( 0 ), t0( 0 ), t1( 0 )
      {}

  int do_fill( const int odes, const std::string & filltypes );
  bool read_buffer( const int ides ) throw();
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

  int check_block( const Block & b, int & copied_size, int & error_size );
  int check_all();
  void show_status( const long long ipos, const char * const msg = 0,
                    bool force = false ) throw();
public:
  Genbook( const long long ipos, const long long opos, Domain & dom,
           const long long isize, const char * const logname,
           const int cluster, const int hardbs )
    : Logbook( ipos, opos, dom, isize, logname, cluster, hardbs, false ),
      a_rate( 0 ), c_rate( 0 ), first_size( 0 ), last_size( 0 ),
      last_ipos( 0 ), t0( 0 ), t1( 0 ), oldlen( 0 ) {}

  int do_generate( const int odes );
  };


class Rescuebook : public Logbook
  {
  const long long max_error_rate_, min_read_rate_;
  long long sparse_size;		// end position of pending writes
  long long recsize, errsize;		// total recovered and error sizes
  const char * const iname_;
  const int max_retries_, skipbs_;
  int max_errors_;
  int e_code;				// error code for too many errors
  int errors;				// error areas found so far
  int ides_, odes_;			// input and output file descriptors
  const bool nosplit_, sparse_, synchronous_;
					// variables for update_status
  long long a_rate, c_rate, first_size, last_size, last_errsize;
  long long last_ipos;
  long t0, t1, ts;
  int oldlen;
  bool status_changed;

  int skipbs() const throw() { return skipbs_; }
  bool sync_sparse_file() throw();
  int copy_block( const Block & b, int & copied_size, int & error_size );
  void count_errors() throw();
  bool too_many_errors() throw()
    { if( max_errors_ >= 0 && errors > max_errors_ ) e_code |= 2;
      return ( e_code != 0 ); }
  bool slow_read() const throw()
    { return ( ( min_read_rate_ > 0 && c_rate < min_read_rate_ ) ||
               ( min_read_rate_ == 0 && c_rate < a_rate / 10 ) ); }
  int copy_and_update( const Block & b, const Sblock::Status st,
                       int & copied_size, int & error_size,
                       const char * const msg, bool & first_post );
  int copy_non_tried();
  int rcopy_non_tried();
  int trim_errors();
  int rtrim_errors();
  int split_errors();
  int rsplit_errors();
  int copy_errors();
  int rcopy_errors();
  void update_status() throw();
  void show_status( const long long ipos, const char * const msg = 0,
                    const bool force = false ) throw();
public:
  Rescuebook( const long long ipos, const long long opos,
              Domain & dom, const long long isize,
              const char * const iname, const char * const logname,
              const int cluster, const int hardbs,
              const long long max_error_rate, const int max_errors = -1,
              const int max_retries = 0, const long long min_read_rate = -1,
              const bool complete_only = false,
              const bool new_errors_only = false, const bool nosplit = false,
              const bool retrim = false, const bool sparse = false,
              const bool synchronous = false, const bool try_again = false );

  int do_rescue( const int ides, const int odes, const bool reverse );
  };


// Defined in io.cc
//
const char * format_num( long long num, long long limit = 999999,
                         const int set_prefix = 0 ) throw();
void set_signals() throw();


// Defined in main.cc ddrescuelog.cc
//
extern int verbosity;
void internal_error( const char * const msg ) __attribute__ ((noreturn));
void show_error( const char * const msg,
                 const int errcode = 0, const bool help = false ) throw();
void write_logfile_header( FILE * const f ) throw();
