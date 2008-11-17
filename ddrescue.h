/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Antonio Diaz Diaz.

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

#ifndef LLONG_MAX
#define LLONG_MAX  0x7FFFFFFFFFFFFFFFLL
#endif
#ifndef LLONG_MIN
#define LLONG_MIN  (-LLONG_MAX - 1LL)
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX 0xFFFFFFFFFFFFFFFFULL
#endif


class Logbook
  {
public:
  enum Status
    { copying = '?', trimming = '*', splitting = '/', retrying = '-',
      filling = 'F', generating = 'G', finished = '+' };

private:
  const long long _offset;		// outfile offset (opos - ipos);
  long long _current_pos;
  Status _current_status;
  Domain & _domain;			// rescue domain
  char *iobuf_base, *_iobuf;		// iobuf is aligned to page and hardbs
  const char * const _filename;
  const int _hardbs, _softbs;
  const char * _final_msg;
  int _final_errno;
  mutable int _index;			// cached index of last find or change
  std::vector< Sblock > sblock_vector;	// note: blocks are consecutive

  void erase_sblock( const int i ) throw()
    { sblock_vector.erase( sblock_vector.begin() + i ); }
  void insert_sblock( const int i, const Sblock & sb ) throw()
    { sblock_vector.insert( sblock_vector.begin() + i, sb ); }

public:
  Logbook( const long long ipos, const long long opos, Domain & dom,
           const long long isize,
           const char * name, const int cluster, const int hardbs,
           const bool complete_only ) throw();
  ~Logbook() throw() { delete[] iobuf_base; }

  bool blank() const throw();
  void compact_sblock_vector() throw();
  void split_domain_border_sblocks() throw();
  bool update_logfile( const int odes = -1, const bool force = false ) throw();

  long long current_pos() const throw() { return _current_pos; }
  Status current_status() const throw() { return _current_status; }
  const Domain & domain() const throw() { return _domain; }
  const char *filename() const throw() { return _filename; }
  char * iobuf() const throw() { return _iobuf; }
  int hardbs() const throw() { return _hardbs; }
  int softbs() const throw() { return _softbs; }
  long long offset() const throw() { return _offset; }
  const char * final_msg() const throw() { return _final_msg; }
  int final_errno() const throw() { return _final_errno; }

  void current_pos( const long long pos ) throw() { _current_pos = pos; }
  void current_status( Status st ) throw() { _current_status = st; }
  void final_msg( const char * const msg ) throw() { _final_msg = msg; }
  void final_errno( const int e ) throw() { _final_errno = e; }

  const Sblock & sblock( const int i ) const throw() { return sblock_vector[i]; }
  int sblocks() const throw() { return sblock_vector.size(); }
  void change_sblock_status( const int i, const Sblock::Status st ) throw()
    { sblock_vector[i].status( st ); }
  void truncate_vector( const long long pos ) throw();

  int find_index( const long long pos ) const throw();
  void find_chunk( Block & b, const Sblock::Status st ) const throw();
  void rfind_chunk( Block & b, const Sblock::Status st ) const throw();
  void change_chunk_status( const Block & b, const Sblock::Status st ) throw();

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
  int _odes;				// output file descriptor
  const bool _synchronous;

  int fill_areas( const std::string & filltypes ) throw();
  int fill_block( const Block & b ) throw();
  void show_status( const long long ipos, bool force = false ) throw();

public:
  Fillbook( const long long ipos, const long long opos, Domain & dom,
            const char * name,
            const int cluster, const int hardbs,
            const bool synchronous ) throw()
    : Logbook( ipos, opos, dom, 0, name, cluster, hardbs, true ),
      _synchronous( synchronous ) {}

  int do_fill( const int odes, const std::string & filltypes ) throw();
  bool read_buffer( const int ides ) throw();
  };


class Rescuebook : public Logbook
  {
  long long sparse_size;		// end position of pending writes
  long long recsize, errsize;		// total recovered and error sizes
  int errors;				// error areas found so far
  int _ides, _odes;			// input and output file descriptors
  const int _max_errors, _max_retries;
  const bool _nosplit, _sparse, _synchronous;

  bool sync_sparse_file() throw();
  int check_block( const Block & b, int & copied_size, int & error_size ) throw();
  int copy_block( const Block & b, int & copied_size, int & error_size ) throw();
  int check_all() throw();
  void count_errors() throw();
  bool too_many_errors() const throw()
    { return ( _max_errors >= 0 && errors > _max_errors ); }
  int copy_and_update( const Block & b, const Sblock::Status st,
                       int & copied_size, int & error_size,
                       const char * msg, bool & first_post ) throw();
  int copy_non_tried() throw();
  int trim_errors() throw();
  int split_errors() throw();
  int copy_errors() throw();
  void show_status( const long long ipos, const char * msg = 0,
                    bool force = false ) throw();
public:
  Rescuebook( const long long ipos, const long long opos,
              Domain & dom, const long long isize,
              const char * name, const int cluster, const int hardbs,
              const int max_errors = -1, const int max_retries = 0,
              const bool complete_only = false, const bool nosplit = false,
              const bool retrim = false, const bool sparse = false,
              const bool synchronous = false ) throw();

  int do_generate( const int odes ) throw();
  int do_rescue( const int ides, const int odes ) throw();
  };


// Defined in ddrescue.cc
//
const char * format_num( long long num, long long max = 999999,
                         const int set_prefix = 0 ) throw();
void set_handler() throw();


// Defined in main.cc
//
extern int verbosity;
void internal_error( const char * msg ) throw() __attribute__ ((noreturn));
void show_error( const char * msg,
                 const int errcode = 0, const bool help = false ) throw();
void write_logfile_header( FILE * f ) throw();
