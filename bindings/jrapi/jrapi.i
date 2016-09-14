

/* Java SWIG interface for rapi.h */

%module Rapi

/*
Remove rapi_ prefix from all function and structure names.
Make sure this general rename comes before the ignore clauses below
or they won't have effect.
*/

%rename("AlignerState") "rapi_aligner_state";
%rename("Alignment")    "rapi_alignment";
%rename("Batch")        "rapi_batch_wrap";
%rename("Contig")       "rapi_contig";
%rename("Opts")         "rapi_opts";
%rename("Read")         "rapi_read";
%rename("Ref")          "rapi_ref";

%rename("%(strip:[rapi_])s") ""; // e.g., rapi_load_ref -> load_ref
// rename n_ names to camelcase
%rename("%(lowercamelcase)s", regextarget=1) "^n_\w+";

// ignore functions from klib
%rename("$ignore", regextarget=1) "^k";
%rename("$ignore", regextarget=1) "^K";

// also ignore anything that starts with an underscore
%rename("$ignore", regextarget=1) "^_";

// Swig doesn't support applying multiple renaming rules to the same identifier.
// To get camel case, we'll need to rename the wrapped classes individually.
// %rename("%(camelcase)s", %$isclass) "";

%header %{
/* includes injected into the C wrapper code.  */
#include <stddef.h>
#include <stdio.h>
#include <rapi.h>
#include <rapi_utils.h>

%}


%include "stdint.i"; // needed to tell swig about types such as uint32_t, uint8_t, etc.


%header %{
#define FQ_EXCEPTION_NAME(name) "it/crs4/rapi/" #name

const char* rapi_err_to_except(rapi_error_t error_code)
{
  switch (error_code) {
    case RAPI_OP_NOT_SUPPORTED_ERROR:
      return FQ_EXCEPTION_NAME(RapiOpNotSupportedException);

    case RAPI_MEMORY_ERROR:
      return FQ_EXCEPTION_NAME(RapiOutOfMemoryError);

    case RAPI_PARAM_ERROR:
      return FQ_EXCEPTION_NAME(RapiInvalidParamException);

    case RAPI_TYPE_ERROR:
      return FQ_EXCEPTION_NAME(RapiInvalidTypeException);

    default:
    case RAPI_GENERIC_ERROR:
      return FQ_EXCEPTION_NAME(RapiException);
  }
}

void do_rapi_throw(JNIEnv *jenv, rapi_error_t err_code, const char* msg)
{
    const char* ex_name = rapi_err_to_except(err_code);
    jclass clazz = (*jenv)->FindClass(jenv, ex_name);
    if (clazz) {
      (*jenv)->ThrowNew(jenv, clazz, msg);
    }
    else {
      char new_msg[1024];
      snprintf(new_msg, sizeof(new_msg), "BUG! Tried to throw %s but failed to find the exception class", ex_name);
      clazz = (*jenv)->FindClass(jenv, "java/lang/RuntimeException");
      (*jenv)->ThrowNew(jenv, clazz, new_msg);
    }
}

/* malloc wrapper that sets a SWIG error in case of failure. */
void* rapi_malloc(JNIEnv* jenv, size_t nbytes) {
  void* result = malloc(nbytes);
  if (!result)
    do_rapi_throw(jenv, RAPI_MEMORY_ERROR, "Failed to allocate memory");

  return result;
}

%}


// With this typemap we can create wrapper functions that receive the JNIEnv* as an argument,
// but hiding it from the Java-side API.
%typemap(in, numinputs=0) JNIEnv* jenv { $1 = jenv; }


/* Eliminate the rapi_error_t return value from all functions, turning
   it into a 'void'.  Later we'll have to add error checking code to
   throw a Java exception then an error is generated by the wrapped C function.
*/
%typemap(out) rapi_error_t ""

%typemap(jstype) rapi_error_t "$typemap(jstype,void)"
%typemap(jtype) rapi_error_t "$typemap(jtype,void)"
%typemap(jni) rapi_error_t "$typemap(jni,void)";
%typemap(javaout) rapi_error_t "$typemap(javaout,void)";


// This macro inserts error checking code (to raise a Java exception)
%define CHECK_RAPI_ERROR(msg)
  if (result != RAPI_NO_ERROR) {
    do_rapi_throw(jenv, result, msg);
    //return $null; XXX: I don't think we can return from here.  Swig places resource clean-up code after the snippet
  }
%enddef

// This macro inserts error checking code (to raise a Java exception) AND
// adds the 'throws' clause.  The clause will indicate a RapiException, though
// the function may also throw any of the exceptions derived from RapiException.
%define Set_exception_from_error_t(fn_name)
%javaexception("RapiException") fn_name {
  $action
  CHECK_RAPI_ERROR("")
}
%enddef



/**************************************************
 ###  Typemaps
 **************************************************/

/*
rapi_error_t is the error type for the RAPI library.
*/
typedef int rapi_error_t;
typedef long long rapi_ssize_t;

%inline %{
typedef int rapi_bool;
%}

%typemap(jtype) rapi_bool "boolean";
%typemap(jstype) rapi_bool "boolean";
%typemap(jni) rapi_bool "jboolean";

%mutable;

/**************************************************
 ###  Include rapi_common.i
 * Include it after we've defined all renaming rules, features,
 * typemaps, etc.
 **************************************************/
%include "../rapi_common.i"
/**************************************************
 **************************************************/


%immutable;

/************ begin wrapping functions and structures **************/

%mutable;
/********* rapi_opts *******/
typedef struct rapi_opts {
  int ignore_unsupported;
  /* Standard Ones - Differently implemented by aligners*/
  int mapq_min;
  int isize_min;
  int isize_max;
  int n_threads;
  rapi_bool share_ref_mem;

  /* Mismatch / Gap_Opens / Quality Trims --> Generalize ? */

  // TODO: how to wrap this thing?
  //kvec_t(rapi_param) parameters;
} rapi_opts;

%extend rapi_opts {
  rapi_opts(JNIEnv* jenv) {
    rapi_opts* tmp = rapi_malloc(jenv, sizeof(rapi_opts));
    if (!tmp) return NULL;

    rapi_error_t error = rapi_opts_init(tmp);
    if (RAPI_NO_ERROR != error) {
      do_rapi_throw(jenv, error, "Error initializing RAPI options structure");
      free(tmp);
      return NULL;
    }

    return tmp;
  }

  ~rapi_opts(void) {
    rapi_error_t error = rapi_opts_free($self);
    if (error != RAPI_NO_ERROR)
      PERROR("Problem destroying opts object (error code %d)\n", error);
  }
};


/****** IMMUTABLE *******/
// Everything from here down is read-only

%immutable;

///* Init and tear down library */
Set_exception_from_error_t(rapi_init)
rapi_error_t rapi_init(const rapi_opts* opts);

Set_exception_from_error_t(rapi_shutdown)
rapi_error_t rapi_shutdown(void);

/*
The char* returned by the following functions are wrapped automatically by
SWIG -- the wrapper doesn't try to free the strings.
*/
const char* rapi_aligner_name(void);
const char* rapi_aligner_version(void);
const char* rapi_plugin_version(void);

/***************************************
 ****** rapi_contig and rapi_ref *******
 ***************************************/


// rapi_contig provides a view into the rapi_ref.  It must
// not be freed, and it should only be constructed by the rapi_ref.
%nodefaultctor rapi_contig;
%nodefaultdtor rapi_contig;

typedef struct rapi_contig {
  char * name;
  uint32_t len;
  char * assembly_identifier;
  char * species;
  char * uri;
  char * md5;
} rapi_contig;


%nodefaultctor rapi_ref;
typedef struct rapi_ref {
  char * path;
} rapi_ref;

%exception rapi_ref::getContig {
  $action
  if (result == NULL) {
    do_rapi_throw(jenv, RAPI_PARAM_ERROR, "index out of bounds");
  }
}

%extend rapi_ref {
  rapi_ref(JNIEnv* jenv, const char* reference_path) {
    if (reference_path == NULL) {
        do_rapi_throw(jenv, RAPI_TYPE_ERROR, "Reference path cannot be None");
        return NULL;
    }

    rapi_ref* ref = (rapi_ref*) rapi_malloc(jenv, sizeof(rapi_ref));
    if (!ref) return NULL;

    int error = rapi_ref_load(reference_path, ref);
    if (error == RAPI_NO_ERROR)
      return ref;
    else {
      free(ref);
      const char* msg;
      if (error == RAPI_MEMORY_ERROR)
        msg = "Insufficient memory available to load the reference.";
      else if (error == RAPI_GENERIC_ERROR)
        msg = "Library failed to load reference. Check paths and reference format.";
      else
        msg = "";

      do_rapi_throw(jenv, error, msg);
      return NULL;
    }
  }

  void unload(void) {
    rapi_error_t error = rapi_ref_free($self);
    if (error != RAPI_NO_ERROR) {
      PERROR("Problem destroying reference (error code %d)\n", error);
    }
  }

  ~rapi_ref(void) {
    // double frees shouldn't cause any problems
    rapi_ref_unload($self);
  }

  size_t getNContigs(void) const { return $self->n_contigs; }

  /* XXX getContig: I worry about memory management here.  We're returning a pointer to the rapi_ref's
    chunk of memory.  If someone keeps a contig pointer around after freeing the reference there's
    going to be sparks.
  */
  const rapi_contig* getContig(int i) const {
    if (i < 0 || i >= $self->n_contigs) {
      return NULL; // exception raised at higher level in %exception block
    }
    else
      return $self->contigs + i;
  }
};


/***************************************/
/*      Alignments                     */
/***************************************/

%typemap(jni) rapi_cigar_ops  "jobjectArray";
%typemap(jstype) rapi_cigar_ops  "it.crs4.rapi.AlignOp[]"
%typemap(jtype) rapi_cigar_ops  "it.crs4.rapi.AlignOp[]"
//%typemap(javaout) rapi_cigar_ops  "$typemap(javaout,AlignOp[])";

// Map rapi_cigar_ops to an array of cigar ops
%typemap(out, throws="RapiException") rapi_cigar_ops {
    /**
    * Convert rapi_cigar_ops to an array of AlignOp objects
    */
    jclass aln_op_clazz = (*jenv)->FindClass(jenv, "it/crs4/rapi/AlignOp");
    if (!aln_op_clazz) {
      do_rapi_throw(jenv, RAPI_TYPE_ERROR, "Failed to find it/crs4/rapi/AlignOp class");
      return $null;
    }
    // LP: it may be a good idea to create global references to the classes we use and cache
    // repeately accessed class, method, and field ids.
    jmethodID op_constr = (*jenv)->GetMethodID(jenv, aln_op_clazz, "<init>", "(CI)V");
    if (!op_constr) {
      do_rapi_throw(jenv, RAPI_TYPE_ERROR, "Could not fetch AlignOp constructor");
      return $null;
    }

    // $1 is variable of tuple rapi_cigar_ops
    jobjectArray array = (*jenv)->NewObjectArray(jenv, $1.len, aln_op_clazz, NULL);
    if (!array) {
      do_rapi_throw(jenv, RAPI_MEMORY_ERROR, "Failed to allocate array for cigar ops");
      return $null;
    }

    // for each cigar operator in the array, map it to an AlignOp object and insert it in the return array
    for (int i = 0; i < $1.len; ++i) {
        rapi_cigar cig = $1.ops[i];
        char c = rapi_cigops_char[cig.op];
        int len = cig.len;

        jobject op = (*jenv)->NewObject(jenv, aln_op_clazz, op_constr, c, len);
        if (!op) {
          do_rapi_throw(jenv, RAPI_MEMORY_ERROR, "Failed to create new AlignOp object");
          return $null;
        }
        (*jenv)->SetObjectArrayElement(jenv, array, i, op);
    }

    $result = array;
}

/* These 2 typemaps handle the conversion of the jtype to jstype typemap type
   and vice versa */
%typemap(javain) rapi_cigar_ops "$javainput"
%typemap(javaout) rapi_cigar_ops {
    return $jnicall;
}

%{
typedef struct {
    rapi_cigar* ops;
    int len;
} rapi_cigar_ops;
%}


%nodefaultctor rapi_alignment;
// Alignments are not to be deleted.  They are attached to the read, which is
// attached to the batch.  When the batch is freed so is all the rest.
%nodefaultdtor rapi_alignment;

typedef struct rapi_alignment {
  rapi_contig* contig;
  long int pos; // 1-based
  short mapq;
  int score; // aligner-specific score

  short n_mismatches;
  short n_gap_opens;
  short n_gap_extensions;
} rapi_alignment;



%newobject rapi_alignment::getCigarString;
%extend rapi_alignment {

    // synthesize boolean attributes corresponding to bitfield values
    rapi_bool paired;
    rapi_bool propPaired;
    rapi_bool mapped;
    rapi_bool reverseStrand;
    rapi_bool secondaryAln;

    rapi_cigar_ops getCigarOps(void) const { // rapi_cigar_ops is then converted to AlignOp[]
        rapi_cigar_ops array;
        array.ops = $self->cigar_ops;
        array.len = $self->n_cigar_ops;
        return array;
    }

    char* getCigarString(void) const {
        kstring_t output = { 0, 0, NULL };
        rapi_put_cigar($self->n_cigar_ops, $self->cigar_ops, 0, &output);
        // return the string directly. Wrapper will be responsible for freeing it (through %newobject)
        return output.s;
    }
/*
    rapi_tag_list get_tags(void) const {
        return $self->tags;
    }
*/
    int get_rlen(void) const {
      return rapi_get_rlen($self->n_cigar_ops, $self->cigar_ops);
    }
};


%{
rapi_bool rapi_alignment_paired_get(const rapi_alignment* aln) {
    return aln->paired != 0;
}

rapi_bool rapi_alignment_propPaired_get(const rapi_alignment* aln) {
    return aln->prop_paired != 0;
}

rapi_bool rapi_alignment_mapped_get(const rapi_alignment* aln) {
    return aln->mapped != 0;
}

rapi_bool rapi_alignment_reverseStrand_get(const rapi_alignment* aln) {
    return aln->reverse_strand != 0;
}

rapi_bool rapi_alignment_secondaryAln_get(const rapi_alignment* aln) {
    return aln->secondary_aln != 0;
}
%}


/***************************************/
/*      Reads and read batches         */
/***************************************/
%nodefaultctor rapi_read;

// Reads are not to be deleted.  They are attached to the read, which is
// attached to the batch.  When the batch is freed so is all the rest.
%nodefaultdtor rapi_read;

typedef struct rapi_read {
  char * id;
  char * seq;
  char * qual;
  unsigned int length;
} rapi_read;

// Raise IndexError when rapi_read.get_aln returns NULL
%exception rapi_read::getAln {
    $action
    if (result == NULL) {
        do_rapi_throw(jenv, RAPI_PARAM_ERROR, "Alignment index out of bounds");
    }
}

%extend rapi_read {
  // synthesize some high-level boolean attributes.  These
  // look at the "main" alignment (first one, if any)
  rapi_bool propPaired;
  rapi_bool mapped;
  rapi_bool reverseStrand;
  int score;
  short mapq;

  int getNAlignments(void) const { return $self->n_alignments; }

  const rapi_alignment* getAln(int index) const {
    if (index >= 0 && index < $self->n_alignments)
      return $self->alignments + index;
    else
      return NULL; // exception raised in %exception block
  }
};


%{
rapi_bool rapi_read_propPaired_get(const rapi_read* read) {
    return read->n_alignments > 0 && rapi_alignment_propPaired_get(read->alignments);
}

rapi_bool rapi_read_mapped_get(const rapi_read* read) {
    return read->n_alignments > 0 && rapi_alignment_mapped_get(read->alignments);
}

rapi_bool rapi_read_reverseStrand_get(const rapi_read* read) {
    return read->n_alignments > 0 && rapi_alignment_reverseStrand_get(read->alignments);
}

int rapi_read_score_get(const rapi_read* read) {
    if (read->n_alignments <= 0) {
        return 0;
    }
    else {
        return read->alignments->score;
    }
}

short rapi_read_mapq_get(const rapi_read* read) {
    if (read->n_alignments <= 0) {
        return 0;
    }
    else {
        return read->alignments->mapq;
    }
}

%}


%{ // this declaration is inserted in the C code
typedef struct rapi_batch_wrap {
  rapi_batch* batch;
  rapi_ssize_t len; // number of reads inserted in batch (as opposed to the space reserved)
} rapi_batch_wrap;
%}

%{
int rapi_batch_wrap_n_reads_per_frag_get(const rapi_batch_wrap* self) {
    return self->batch->n_reads_frag;
}

int rapi_batch_wrap_n_fragments_get(const rapi_batch_wrap* self) {
    return self->len / self->batch->n_reads_frag;
}

int rapi_batch_wrap_length_get(const rapi_batch_wrap* self) {
    return self->len;
}

rapi_ssize_t rapi_batch_wrap_capacity_get(const rapi_batch_wrap* wrap) {
  return rapi_batch_read_capacity(wrap->batch);
}
%}

// This one to the SWIG interpreter.
// We don't expose any of the struct members through SWIG.
%nodefaultctor rapi_batch_wrap;
typedef struct {
} rapi_batch_wrap;


%exception rapi_batch_wrap::getRead {
  $action
  if (result == NULL) {
    do_rapi_throw(jenv, RAPI_PARAM_ERROR, "co-ordinates out of bounds");
  }
}

Set_exception_from_error_t(rapi_batch_wrap::reserve);
Set_exception_from_error_t(rapi_batch_wrap::append);
Set_exception_from_error_t(rapi_batch_wrap::clear);
Set_exception_from_error_t(rapi_batch_wrap::setRead);

%extend rapi_batch_wrap {
  /**
   * Creates a new read_batch for fragments composed of `n_reads_per_frag` reads.
   * The function doesn't pre-allocate any space for reads, so either use `append`
   * to insert reads or call `reserve` before calling `set_read`.
   */
  rapi_batch_wrap(JNIEnv* jenv, int n_reads_per_frag) {
    if (n_reads_per_frag <= 0) {
      do_rapi_throw(jenv, RAPI_PARAM_ERROR, "number of reads per fragment must be greater than or equal to 0");
      return NULL;
    }

    rapi_batch_wrap* wrapper = (rapi_batch_wrap*) rapi_malloc(jenv, sizeof(rapi_batch_wrap));
    if (!wrapper) return NULL;

    wrapper->batch = (rapi_batch*) rapi_malloc(jenv, sizeof(rapi_batch));
    if (!wrapper->batch) {
      free(wrapper);
      return NULL;
    }

    wrapper->len = 0;

    rapi_error_t error = rapi_reads_alloc(wrapper->batch, n_reads_per_frag, 0); // zero-sized allocation to initialize

    if (error != RAPI_NO_ERROR) {
      free(wrapper->batch);
      free(wrapper);
      do_rapi_throw(jenv, RAPI_MEMORY_ERROR, "Error allocating space for reads");
      return NULL;
    }
    else
      return wrapper;
  }

  ~rapi_batch_wrap(void) {
    int error = rapi_reads_free($self->batch);
    free($self);
    if (error != RAPI_NO_ERROR) {
      PERROR("Problem destroying read batch (error code %d)\n", error);
      // TODO: should we raise exceptions in case of errors when freeing/destroying?
    }
  }

  /** Number of reads per fragment */
  const int n_reads_per_frag;

  /** Number of complete fragments inserted */
  const int n_fragments;

  /** Number of reads for which we have allocated memory. */
  const rapi_ssize_t capacity;

  /** Number of reads inserted in batch (as opposed to the space reserved).
   *  This is actually index + 1 of the "forward-most" read to have been inserted.
   */
  const rapi_ssize_t length;

  const rapi_read* getRead(JNIEnv* jenv, rapi_ssize_t n_fragment, int n_read) const
  {
    // Since the underlying rapi code merely checks whether we're indexing
    // allocated space, we precede it with an additional check whether we're
    // accessing space where reads have been inserted.
    //
    // * In both cases, if the returned value is NULL an IndexError is raised
    // in the %exception block
    if (n_fragment * $self->batch->n_reads_frag + n_read >= $self->len) {
        do_rapi_throw(jenv, RAPI_PARAM_ERROR, "Index out of bounds");
        return NULL;
    }

    return rapi_get_read($self->batch, n_fragment, n_read);
  }

  rapi_error_t reserve(rapi_ssize_t n_reads) {
    if (n_reads < 0) {
        PERROR("n_reads must be >= 0");
        return RAPI_PARAM_ERROR;
    }

    rapi_ssize_t n_fragments = n_reads / $self->batch->n_reads_frag;
    // If the reads don't fit completely in n_fragments, add one more
    if (n_reads % $self->batch->n_reads_frag != 0)
      n_fragments +=  1;

    rapi_error_t error = rapi_reads_reserve($self->batch, n_fragments);
    return error;
  }

  rapi_error_t append(const char* id, const char* seq, const char* qual, int q_offset)
  {
    rapi_error_t error = RAPI_NO_ERROR;

    // if id or seq are NULL set them to the empty string and pass them down to the plugin.
    if (!id) id = "";
    if (!seq) seq = "";

    rapi_ssize_t fragment_num = $self->len / $self->batch->n_reads_frag;
    int read_num = $self->len % $self->batch->n_reads_frag;

    rapi_ssize_t read_capacity = rapi_batch_wrap_capacity_get($self);
    if (read_capacity < $self->len + 1) {
      // double the space
      rapi_ssize_t new_capacity = read_capacity > 0 ? read_capacity * 2 : 2;
      error = rapi_batch_wrap_reserve($self, new_capacity);
      if (error != RAPI_NO_ERROR) {
        return error;
      }
    }
    error = rapi_set_read($self->batch, fragment_num, read_num, id, seq, qual, q_offset);
    if (error != RAPI_NO_ERROR) {
      return error;
    }
    else
      ++$self->len;

    return error;
  }

  rapi_error_t clear(void) {
    rapi_error_t error = rapi_reads_clear($self->batch);
    if (error == RAPI_NO_ERROR)
      $self->len = 0;
    return error;
  }

/*  XXX:  maybe we shouldn't expose this method
  rapi_error_t setRead(rapi_ssize_t n_frag, int n_read, const char* id, const char* seq, const char* qual, int q_offset)
  {
    // if id or seq are NULL set them to the empty string and pass them down to the plugin.
    if (!id) id = "";
    if (!seq) seq = "";

    rapi_error_t error = rapi_set_read($self->batch, n_frag, n_read, id, seq, qual, q_offset);
    if (error != RAPI_NO_ERROR) {
      return error;
    }
    else {
      // If the user set a read that is beyond the current batch length, reset the
      // batch length to the new limit.
      rapi_ssize_t index = n_frag * $self->batch->n_reads_frag + n_read;
      if ($self->len <= index)
        $self->len = index + 1;
    }
    return error;
  }
*/
}

/***************************************/
/*      The aligner                    */
/***************************************/

%{ // forward declaration of opaque structure (in C-code)
struct rapi_aligner_state;
%}

%nodefaultctor  rapi_aligner_state;

typedef struct rapi_aligner_state {} rapi_aligner_state; //< opaque structure.  Aligner can use for whatever it wants.

Set_exception_from_error_t(rapi_aligner_state::alignReads);

%extend rapi_aligner_state {
  rapi_aligner_state(JNIEnv* jenv, const rapi_opts* opts)
  {
    struct rapi_aligner_state* p_state;
    rapi_error_t error = rapi_aligner_state_init(&p_state, opts);

    if (RAPI_NO_ERROR != error) {
      do_rapi_throw(jenv, error, "Failed to create aligner state");
      return NULL;
    }
    return p_state;
  }

  ~rapi_aligner_state(void) {
    rapi_error_t error = rapi_aligner_state_free($self);
    if (error != RAPI_NO_ERROR)
      PERROR("Problem destroying aligner state object (error code %d)\n", error);
  }
  
  rapi_error_t alignReads(JNIEnv* jenv, const rapi_ref* ref, rapi_batch_wrap* batch)
  {
    if (NULL == ref || NULL == batch) {
      PERROR("ref and batch arguments must not be NULL\n");
      return RAPI_PARAM_ERROR;
    }

    if (batch->len % batch->batch->n_reads_frag != 0) {
      PERROR("Incomplete fragment in batch! Number of reads appended (%lld) is not a multiple of the number of reads per fragment (%d)\n",
        batch->len, batch->batch->n_reads_frag);
      return RAPI_GENERIC_ERROR;
    }

    rapi_ssize_t start_fragment = 0;
    rapi_ssize_t end_fragment = batch->len / batch->batch->n_reads_frag;
    return rapi_align_reads(ref, batch->batch, start_fragment, end_fragment, $self);
  }
};

/***************************************/
/*      SAM output                     */
/***************************************/

%newobject format_sam_hdr;
%inline %{
char* format_sam_hdr(JNIEnv* jenv, const rapi_ref* ref)
{
  kstring_t output = { 0, 0, NULL };
  rapi_error_t error = rapi_format_sam_hdr(ref, &output);
  if (error == RAPI_NO_ERROR) {
    return output.s;
  }
  else {
    do_rapi_throw(jenv, error, "Failed to format SAM header");
    return NULL;
  }
}
%}


// create overloaded `format_sam_batch` functions -- one that accepts
// a fragment index and one that doesn't, but is applied to the entire batch.
%rename(format_sam_batch) format_sam_batch2;

%newobject format_sam_batch;
%newobject format_sam_batch2;

%inline %{
char* format_sam_batch(JNIEnv* jenv, const rapi_batch_wrap* reads, rapi_ssize_t frag_idx)
{
  if (!reads) {
    do_rapi_throw(jenv, RAPI_PARAM_ERROR, "NULL read_batch pointer!");
    return NULL;
  }

  if (frag_idx >= reads->len / reads->batch->n_reads_frag) {
    do_rapi_throw(jenv, RAPI_PARAM_ERROR, "Index value out of range");
    return NULL;
  }

  int start, end;

  if (frag_idx < 0) {
    start = 0;
    end = reads->len / reads->batch->n_reads_frag;
  }
  else {
    start = frag_idx;
    end = start + 1;
  }

  kstring_t output = { 0, 0, NULL };
  rapi_error_t error = RAPI_NO_ERROR;

  for (rapi_ssize_t i = start; i < end && error == RAPI_NO_ERROR; ++i) {
    error = rapi_format_sam_b(reads->batch, i, &output);
    kputc('\n', &output);
  }
  if (error == RAPI_NO_ERROR) {
    return output.s;
  }
  else {
    free(output.s);
    do_rapi_throw(jenv, error, "Failed to format SAM");
    return NULL;
  }
}

char* format_sam_batch2(JNIEnv* jenv, const rapi_batch_wrap* reads)
{
  return format_sam_batch(jenv, reads, -1);
}
%}
