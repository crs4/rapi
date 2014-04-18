/*
 * rapi_bwa.c
 */

#include "../../aligner.h"
#include <bwamem.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/**
 * The 'bwa_pg' string is statically allocated in some BWA file that we're not
 * linking, so we need to define it here.  I think it's the string that the
 * program uses to identify itself in the SAM header's @PG tag.
 */
char* bwa_pg = "rapi";

/******** Utility functions *******/
void aln_print_read(FILE* out, const aln_read* read)
{
	fprintf(out, "read id: %s\n", read->id);
	fprintf(out, "read length: %d\n", read->length);
	fprintf(out, "read seq: %s\n", read->seq);
	fprintf(out, "read qual: %s\n", read->qual);
}

/*
 * Format SAM for read, given the alignment at index `which_aln`
 * (currently in aln_read we only have a single alignment instead of a list,
 * so this should always be 0).
 *
 * \param str_handle ptr to ptr to a string buffer. If not allocated, then
 *     *str_handle == NULL. Else, *str_handle must point to a valid buffer
 *     of length `buflen` allocated with malloc.  It may be resized with,
 *     realloc, in which case *str_handle will be reset accordingly.
 */
void aln_format_sam(aln_read* read, aln_read* mate, int which_aln, char**str_handle, int buflen)
{
#if 0
	/**** code taken from mem_aln2sam in BWA ***/
	// set flag
	p->flag |= m? 0x1 : 0; // is paired in sequencing
	p->flag |= p->rid < 0? 0x4 : 0; // is mapped
	p->flag |= m && m->rid < 0? 0x8 : 0; // is mate mapped
	if (p->rid < 0 && m && m->rid >= 0) // copy mate to alignment
		p->rid = m->rid, p->pos = m->pos, p->is_rev = m->is_rev, p->n_cigar = 0;
	if (m && m->rid < 0 && p->rid >= 0) // copy alignment to mate
		m->rid = p->rid, m->pos = p->pos, m->is_rev = p->is_rev, m->n_cigar = 0;
	p->flag |= p->is_rev? 0x10 : 0; // is on the reverse strand
	p->flag |= m && m->is_rev? 0x20 : 0; // is mate on the reverse strand


	kputs(s->name, str); kputc('\t', str); // QNAME
	kputw((p->flag&0xffff) | (p->flag&0x10000? 0x100 : 0), str); kputc('\t', str); // FLAG
	if (p->rid >= 0) { // with coordinate
		kputs(bns->anns[p->rid].name, str); kputc('\t', str); // RNAME
		kputl(p->pos + 1, str); kputc('\t', str); // POS
		kputw(p->mapq, str); kputc('\t', str); // MAPQ
		if (p->n_cigar) { // aligned
			for (i = 0; i < p->n_cigar; ++i) {
				int c = p->cigar[i]&0xf;
				if (c == 3 || c == 4) c = which? 4 : 3; // use hard clipping for supplementary alignments
				kputw(p->cigar[i]>>4, str); kputc("MIDSH"[c], str);
			}
		} else kputc('*', str); // having a coordinate but unaligned (e.g. when copy_mate is true)
	} else kputsn("*\t0\t0\t*", 7, str); // without coordinte
	kputc('\t', str);

	// print the mate position if applicable
	if (m && m->rid >= 0) {
		if (p->rid == m->rid) kputc('=', str);
		else kputs(bns->anns[m->rid].name, str);
		kputc('\t', str);
		kputl(m->pos + 1, str); kputc('\t', str);
		if (p->rid == m->rid) {
			int64_t p0 = p->pos + (p->is_rev? get_rlen(p->n_cigar, p->cigar) - 1 : 0);
			int64_t p1 = m->pos + (m->is_rev? get_rlen(m->n_cigar, m->cigar) - 1 : 0);
			if (m->n_cigar == 0 || p->n_cigar == 0) kputc('0', str);
			else kputl(-(p0 - p1 + (p0 > p1? 1 : p0 < p1? -1 : 0)), str);
		} else kputc('0', str);
	} else kputsn("*\t0\t0", 5, str);
	kputc('\t', str);

	// print SEQ and QUAL
	if (p->flag & 0x100) { // for secondary alignments, don't write SEQ and QUAL
		kputsn("*\t*", 3, str);
	} else if (!p->is_rev) { // the forward strand
		int i, qb = 0, qe = s->l_seq;
		if (p->n_cigar) {
			if (which && ((p->cigar[0]&0xf) == 4 || (p->cigar[0]&0xf) == 3)) qb += p->cigar[0]>>4;
			if (which && ((p->cigar[p->n_cigar-1]&0xf) == 4 || (p->cigar[p->n_cigar-1]&0xf) == 3)) qe -= p->cigar[p->n_cigar-1]>>4;
		}
		ks_resize(str, str->l + (qe - qb) + 1);
		for (i = qb; i < qe; ++i) str->s[str->l++] = "ACGTN"[(int)s->seq[i]];
		kputc('\t', str);
		if (s->qual) { // printf qual
			ks_resize(str, str->l + (qe - qb) + 1);
			for (i = qb; i < qe; ++i) str->s[str->l++] = s->qual[i];
			str->s[str->l] = 0;
		} else kputc('*', str);
	} else { // the reverse strand
		int i, qb = 0, qe = s->l_seq;
		if (p->n_cigar) {
			if (which && ((p->cigar[0]&0xf) == 4 || (p->cigar[0]&0xf) == 3)) qe -= p->cigar[0]>>4;
			if (which && ((p->cigar[p->n_cigar-1]&0xf) == 4 || (p->cigar[p->n_cigar-1]&0xf) == 3)) qb += p->cigar[p->n_cigar-1]>>4;
		}
		ks_resize(str, str->l + (qe - qb) + 1);
		for (i = qe-1; i >= qb; --i) str->s[str->l++] = "TGCAN"[(int)s->seq[i]];
		kputc('\t', str);
		if (s->qual) { // printf qual
			ks_resize(str, str->l + (qe - qb) + 1);
			for (i = qe-1; i >= qb; --i) str->s[str->l++] = s->qual[i];
			str->s[str->l] = 0;
		} else kputc('*', str);
	}

	// print optional tags
	if (p->n_cigar) {
		kputsn("\tNM:i:", 6, str); kputw(p->NM, str);
		kputsn("\tMD:Z:", 6, str); kputs((char*)(p->cigar + p->n_cigar), str);
	}
	if (p->score >= 0) { kputsn("\tAS:i:", 6, str); kputw(p->score, str); }
	if (p->sub >= 0) { kputsn("\tXS:i:", 6, str); kputw(p->sub, str); }
	if (bwa_rg_id[0]) { kputsn("\tRG:Z:", 6, str); kputs(bwa_rg_id, str); }
	if (!(p->flag & 0x100)) { // not multi-hit
		for (i = 0; i < n; ++i)
			if (i != which && !(list[i].flag&0x100)) break;
		if (i < n) { // there are other primary hits; output them
			kputsn("\tSA:Z:", 6, str);
			for (i = 0; i < n; ++i) {
				const mem_aln_t *r = &list[i];
				int k;
				if (i == which || (list[i].flag&0x100)) continue; // proceed if: 1) different from the current; 2) not shadowed multi hit
				kputs(bns->anns[r->rid].name, str); kputc(',', str);
				kputl(r->pos+1, str); kputc(',', str);
				kputc("+-"[r->is_rev], str); kputc(',', str);
				for (k = 0; k < r->n_cigar; ++k) {
					kputw(r->cigar[k]>>4, str); kputc("MIDSH"[r->cigar[k]&0xf], str);
				}
				kputc(',', str); kputw(r->mapq, str);
				kputc(',', str); kputw(r->NM, str);
				kputc(';', str);
			}
		}
	}
	if (s->comment) { kputc('\t', str); kputs(s->comment, str); }
#endif
}

/**********************************/


/******** Internal structures *****/
typedef struct {
	unsigned long n_bases;
	int n_reads;
	int n_reads_per_frag;
	bseq1_t* seqs;
} bwa_batch;

void _print_bwa_batch(FILE* out, const bwa_batch* read_batch)
{
	fprintf(out, "batch with %ld bases, %d reads, %d reads per fragment",
			read_batch->n_bases, read_batch->n_reads, read_batch->n_reads_per_frag);
	if (read_batch->n_reads_per_frag > 0)
		fprintf(out, " (so %d fragments)\n",	read_batch->n_reads / read_batch->n_reads_per_frag);
	else
		fprintf(out, "\n");

	fprintf(out, "=== Reads: ===\n");
	for (int r = 0; r < read_batch->n_reads; ++r) {
		const bseq1_t* bwa_read = read_batch->seqs + r;
		fprintf(out, "-=-=--=\n");
		fprintf(out, "name: %s\n", bwa_read->name);
		fprintf(out, "seq: %s\n", bwa_read->seq);
		fprintf(out, "qual %.*s\n", bwa_read->l_seq, bwa_read->qual);
	}
}

/**********************************/

/**
 * Definition of the aligner state structure.
 */
struct aln_aligner_state {
	int64_t n_reads_processed;
	// paired-end stats
	mem_pestat_t pes[4];
};

#if 0
aln_kv* aln_kv_insert_char(const char* key, char value,           const aln_kv* tail);
aln_kv* aln_kv_insert_str( const char* key, const char* value,    const aln_kv* tail);
aln_kv* aln_kv_insert_long(const char* key, long value,           const aln_kv* tail);
aln_kv* aln_kv_insert_dbl( const char* key, double value,         const aln_kv* tail);
aln_kv* aln_kv_insert_ba(  const char* key, const uint8_t* value, const aln_kv* tail);
aln_kv* aln_kv_insert_la(  const char* key, const long* value,    const aln_kv* tail);

#endif

int aln_kv_free_list(aln_kv* list) {
	fprintf(stderr, "Warning: aln_kv_free_list NOT IMPLEMENTED\n");
	return ALN_NO_ERROR;
}

#if 1 // strdup is not defined if we compile with c99
char* strdup(const char* str)
{
	if (NULL == str)
		return NULL;

	size_t len = strlen(str);
	char* new_str = malloc(len + 1);
	if (NULL == new_str)
		return NULL;
	return strcpy(new_str, str);
}
#endif

/* Init Library */
int aln_init(const aln_opts* opts)
{
	// no op
	return ALN_NO_ERROR;
}

/* Init Library Options */
int aln_init_opts( aln_opts * my_opts )
{
	// create a BWA opt structure
	mem_opt_t*const bwa_opt = mem_opt_init();

	// Default values copied from bwamem.c in 0.7.8
	bwa_opt->flag = 0;
	bwa_opt->a = 1; bwa_opt->b = 4;
	bwa_opt->o_del = bwa_opt->o_ins = 6;
	bwa_opt->e_del = bwa_opt->e_ins = 1;
	bwa_opt->w = 100;
	bwa_opt->T = 30;
	bwa_opt->zdrop = 100;
	bwa_opt->pen_unpaired = 17;
	bwa_opt->pen_clip5 = bwa_opt->pen_clip3 = 5;
	bwa_opt->min_seed_len = 19;
	bwa_opt->split_width = 10;
	bwa_opt->max_occ = 10000;
	bwa_opt->max_chain_gap = 10000;
	bwa_opt->max_ins = 10000;
	bwa_opt->mask_level = 0.50;
	bwa_opt->chain_drop_ratio = 0.50;
	bwa_opt->split_factor = 1.5;
	bwa_opt->chunk_size = 10000000;
	bwa_opt->n_threads = 1;
	bwa_opt->max_matesw = 100;
	bwa_opt->mask_level_redun = 0.95;
	bwa_opt->mapQ_coef_len = 50; bwa_opt->mapQ_coef_fac = log(bwa_opt->mapQ_coef_len);
	bwa_fill_scmat(bwa_opt->a, bwa_opt->b, bwa_opt->mat);

	my_opts->_private = bwa_opt; // hand the bwa structure onto the external one
	my_opts->ignore_unsupported = 1;
	my_opts->mapq_min     = 0;
	my_opts->isize_min    = 0;
	my_opts->isize_max    = bwa_opt->max_ins;
	my_opts->n_parameters = 0;
	my_opts->parameters   = NULL;

	return ALN_NO_ERROR;
}

int aln_free_opts( aln_opts * my_opts )
{
	free(my_opts->_private);
	return ALN_NO_ERROR;
}

/* Load Reference */
const char * aln_version()
{
	return "my version!";
}

/* Load Reference */
int aln_load_ref( const char * reference_path, aln_ref * ref_struct )
{
	if ( NULL == ref_struct || NULL == reference_path )
		return ALN_PARAM_ERROR;

	const bwaidx_t*const bwa_idx = bwa_idx_load(reference_path, BWA_IDX_ALL);
	if ( NULL == bwa_idx )
		return ALN_REFERENCE_ERROR;

	// allocate memory
	ref_struct->path = strdup(reference_path);
	ref_struct->contigs = calloc( bwa_idx->bns->n_seqs, sizeof(aln_contig) );
	if ( NULL == ref_struct->path || NULL == ref_struct->contigs )
	{
		// if either allocations we free everything and return an error
		bwa_idx_destroy((bwaidx_t*)bwa_idx);
		free(ref_struct->path);
		free(ref_struct->contigs);
		memset(ref_struct, 0, sizeof(*ref_struct));
		return ALN_MEMORY_ERROR;
	}

	/* Fill in Contig Information */
	ref_struct->n_contigs = bwa_idx->bns->n_seqs; /* Handle contains bnt_seq_t * bns holding contig information */
	for ( int i = 0; i < ref_struct->n_contigs; ++i )
	{
		aln_contig* c = &ref_struct->contigs[i];
		c->len = bwa_idx->bns->anns[i].len;
		c->name = bwa_idx->bns->anns[i].name; // points to BWA string
		c->assembly_identifier = NULL;
		c->species = NULL;
		c->uri = NULL;
	}
	ref_struct->_private = (bwaidx_t*)bwa_idx;

	return ALN_NO_ERROR;
}

/* Free Reference */
int aln_free_ref( aln_ref * ref )
{
	// free bwa's part
	bwa_idx_destroy(ref->_private);

	// then free the rest of the structure
	free(ref->path);

	if (ref->contigs) {
		for ( int i = 0; i < ref->n_contigs; ++i )
		{
			aln_contig* c = &ref->contigs[i];
			// *Don't* free name since it points to BWA's string
			free(c->assembly_identifier);
			free(c->species);
			free(c->uri);
		}
		free (ref->contigs);
	}
	memset(ref, 0, sizeof(*ref));
	return ALN_NO_ERROR;
}

void _free_bwa_batch_contents(bwa_batch* batch)
{
	for (int i = 0; i < batch->n_reads; ++i) {
		// *Don't* free the read id since it points to the original structure's string
		free(batch->seqs[i].seq);
		free(batch->seqs[i].qual);
	}

	free(batch->seqs);
	memset(batch, 0, sizeof(bwa_batch));
}


static int _batch_to_bwa_seq(const aln_batch* batch, const aln_opts* opts, bwa_batch* bwa_seqs)
{
	bwa_seqs->n_bases = 0;
	bwa_seqs->n_reads = 0;
	bwa_seqs->n_reads_per_frag = batch->n_reads_frag;
	fprintf(stderr, "Need to allocate %d elements of size %ld\n", batch->n_frags * batch->n_reads_frag, sizeof(bseq1_t));

	bwa_seqs->seqs = calloc(batch->n_frags * batch->n_reads_frag, sizeof(bseq1_t));
	if (NULL == bwa_seqs->seqs) {
		fprintf(stderr, "Allocation failed!\n");
		return ALN_MEMORY_ERROR;
	}

	for (int f = 0; f < batch->n_frags; ++f)
	{
		for (int r = 0; r < batch->n_reads_frag; ++r)
		{
			const aln_read* rapi_read = aln_get_read(batch, f, r);
			bseq1_t* bwa_read = bwa_seqs->seqs + bwa_seqs->n_reads;

			// -- In bseq1_t, all strings are null-terminated.
			// We duplicated the seq and qual since BWA modifies them.
			bwa_read->seq = strdup(rapi_read->seq);
			bwa_read->qual = (NULL == rapi_read->qual) ? NULL : strdup(rapi_read->qual);
			if (!bwa_read->seq || (!rapi_read->qual && bwa_read->qual))
				goto failed_allocation;

			bwa_read->name = rapi_read->id;
			bwa_read->l_seq = rapi_read->length;
			bwa_read->comment = NULL;
			bwa_read->sam = NULL;

			// As we build the objects n_reads keeps the actual number
			// constructed thus far and thus can be used to free the allocated
			// structres in case of error.
			bwa_seqs->n_reads += 1;
			bwa_seqs->n_bases += rapi_read->length;
		}
	}
	return ALN_NO_ERROR;

failed_allocation:
	fprintf(stderr, "Failed to allocate while constructing sequences! Freeing and returning\n");
	_free_bwa_batch_contents(bwa_seqs);
	return ALN_MEMORY_ERROR;
}

/*
 * Many of the default option values need to be adjusted if the matching score
 * (opt->a) is changed.  This function (from the BWA code) does that.
 *
 * \param opt The structure containing actual values and that will be "fixed"
 * \param override set the members to 1 if the value has been overridden and thus should be kept;
 *                 else the corresponding value in opts is assumed to be at default and will be adjusted.
 *
 * So, if you change 'a' set the 'override' accordingly and call this function.  E.g.,
 *
 * <pre>
 *   aln_opts* opt = aln_init_opts();
 *   mem_opt_t* bwa_opts = (mem_opt_t*) opt->_private;
 *   bwa_opts->a = 2;
 *   bwa_opts->b = 5;
 *   mem_opt_t override;
 *   override.a = override.b = 1;
 *   adjust_bwa_opts(bwa_opts, &override);
 * </pre>
 */
void adjust_bwa_opts(mem_opt_t* opt, const mem_opt_t* override)
{
	if (override->a == 1) { // matching score is changed
		if (override->b != 1) opt->b *= opt->a;
		if (override->T != 1) opt->T *= opt->a;
		if (override->o_del != 1) opt->o_del *= opt->a;
		if (override->e_del != 1) opt->e_del *= opt->a;
		if (override->o_ins != 1) opt->o_ins *= opt->a;
		if (override->e_ins != 1) opt->e_ins *= opt->a;
		if (override->zdrop != 1) opt->zdrop *= opt->a;
		if (override->pen_clip5 != 1) opt->pen_clip5 *= opt->a;
		if (override->pen_clip3 != 1) opt->pen_clip3 *= opt->a;
		if (override->pen_unpaired != 1) opt->pen_unpaired *= opt->a;
		bwa_fill_scmat(opt->a, opt->b, opt->mat);
	}
}

static int _convert_opts(const aln_opts* opts, mem_opt_t* bwa_opts)
{
	bwa_opts->T = opts->mapq_min;
	bwa_opts->max_ins = opts->isize_max;

	// TODO: other options provided through 'parameters' field
	return ALN_NO_ERROR;
}

int aln_init_aligner_state(const aln_opts* opts, struct aln_aligner_state** ret_state)
{
	// allocate and zero the structure
	aln_aligner_state* state = *ret_state = calloc(1, sizeof(aln_aligner_state));
	if (NULL == state)
		return ALN_MEMORY_ERROR;
	return ALN_NO_ERROR;
}

int aln_free_aligner_state(aln_aligner_state* state)
{
	free(state);
	return ALN_NO_ERROR;
}

/********** modified BWA code *****************/
extern mem_alnreg_v mem_align1_core(const mem_opt_t *opt, const bwt_t *bwt, const bntseq_t *bns, const uint8_t *pac, int l_seq, char *seq);
// IMPORTANT: must run mem_sort_and_dedup() before calling mem_mark_primary_se function (but it's called by mem_align1_core)
void mem_mark_primary_se(const mem_opt_t *opt, int n, mem_alnreg_t *a, int64_t id);

typedef struct {
	const mem_opt_t *opt;
	const aln_ref* rapi_ref;
	const bwa_batch* read_batch;
	mem_pestat_t *pes;
	mem_alnreg_v *regs;
	int64_t n_processed;
} bwa_worker_t;

/*
 * This function is the same as worker1 from bwamem.c
 */
static void bwa_worker_1(void *data, int i, int tid)
{
	bwa_worker_t *w = (bwa_worker_t*)data;
#if TEST
	fprintf(stderr, "bwa_worker_1 with i %d\n", i);
	fprintf(stderr, "w->opt = %p;          \n" ,  w->opt        );
	fprintf(stderr, "w->read_batch = %p;   \n" ,  w->read_batch );
	fprintf(stderr, "w->regs =  %p;        \n" ,  w->regs       );
	fprintf(stderr, "w->pes = %p;          \n" ,  w->pes        );
	fprintf(stderr, "w->n_processed = %ld;  \n" , w->n_processed);

	fprintf(stderr, "alignment ref path = %s\n" , w->rapi_ref->path);
	fprintf(stderr, "It has %d contigs\n" , w->rapi_ref->n_contigs);
	for (int i = 0; i < w->rapi_ref->n_contigs; ++i)
		fprintf(stderr, "%d: %s\n", i, w->rapi_ref->contigs[i].name);

	fprintf(stderr, "opt->flag is %d\n", w->opt->flag);
#endif
	const bwaidx_t* const bwaidx = (bwaidx_t*)(w->rapi_ref->_private);
	const bwt_t*    const bwt    = bwaidx->bwt;
	const bntseq_t* const bns    = bwaidx->bns;
	const uint8_t*  const pac    = bwaidx->pac;

	fprintf(stderr, "bwa_worker_1: MEM_F_PE is %sset\n", ((w->opt->flag & MEM_F_PE) == 0 ? "not " : " "));
	if (w->opt->flag & MEM_F_PE) {
		int read = 2*i;
		int mate = 2*i + 1;
#if TEST
		fprintf(stderr, "Read: %d: \n", read);
		fprintf(stderr, "(%d)  %s\n", w->read_batch->seqs[read].l_seq, w->read_batch->seqs[read].seq);
		fprintf(stderr, "mate: %d: \n", mate);
		fprintf(stderr, "(%d)  %s\n", w->read_batch->seqs[mate].l_seq, w->read_batch->seqs[mate].seq);
#endif
		w->regs[read] = mem_align1_core(w->opt, bwt, bns, pac, w->read_batch->seqs[read].l_seq, w->read_batch->seqs[read].seq);
		w->regs[mate] = mem_align1_core(w->opt, bwt, bns, pac, w->read_batch->seqs[mate].l_seq, w->read_batch->seqs[mate].seq);
	} else {
		w->regs[i] = mem_align1_core(w->opt, bwt, bns, pac, w->read_batch->seqs[i].l_seq, w->read_batch->seqs[i].seq);
	}
}

/* based on worker2 from bwamem.c */
static void bwa_worker_2(void *data, int i, int tid)
{
	bwa_worker_t *w = (bwa_worker_t*)data;
	fprintf(stderr, "bwa_worker_2 with i %d\n", i);

	const bwaidx_t* const bwaidx = (bwaidx_t*)(w->rapi_ref->_private);
	const bntseq_t* const bns    = bwaidx->bns;
	const uint8_t*  const pac    = bwaidx->pac;

	if ((w->opt->flag & MEM_F_PE)) {
		// paired end
		//mem_sam_pe(w->opt, w->bns, w->pac, w->pes, (w->n_processed>>1) + i, &w->seqs[i<<1], &w->regs[i<<1]);
		//free(w->regs[i<<1|0].a); free(w->regs[i<<1|1].a);
	}
	else {
		// single end
		mem_mark_primary_se(w->opt, w->regs[i].n, w->regs[i].a, w->n_processed + i);
		//mem_reg2sam_se(w->opt, w->bns, w->pac, &w->seqs[i], &w->regs[i], 0, 0);
		//_mem_reg2_rapi_aln_se(w->opt, w->bns, w->pac, &w->seqs[i], &w->regs[i], 0, 0);
		//free(w->regs[i].a);
	}
}

#if 0
/* based on mem_aln2sam */
static int _bwa_aln_to_rapi_aln(const aln_ref* our_ref, aln_read* our_read, int is_paired,
		const bseq1_t *s,
		const mem_aln_t *const bwa_aln_list, int list_length)
{
	const bntseq_t *const bns = (bwaidx_t*)rapi_ref->_private->bns;
	if (list_length < 0)
		return ALN_PARAM_ERROR;

	our_read->alignment = calloc(list_length, sizeof(aln_alignment));
	if (NULL == our_read->alignment)
		return ALN_MEMORY_ERROR;
	our_read->n_alignments = list_length;

	for (int which = 0; which < list_length; ++which)
	{
		const mem_aln_t* bwa_aln = &list[which];
		aln_alignment* our_aln = &our_read->alignment[which];

		if (bwa_aln->rid >= our_ref->n_contigs) { // huh?? Out of bounds
			fprintf(stderr, "read reference id value %d is out of bounds (n_contigs: %d)\n", bwa_aln->rid, our_ref->n_contigs);
			free(our_read->alignment);
			our_read->alignment = our_read->n_alignments = 0;
			return ALN_GENERIC_ERROR;
		}

		// set flags
		our_aln->paired = is_paired != 0;
		// TODO: prop_paired:  how does BWA define a "proper pair"?
		our_aln->prop_paired = 0;
		our_aln->score = bwa_aln->score;
		our_aln->mapq = bwa_aln->mapq;
		our_aln->secondary_aln = bwa_aln->flag & 0x100 != 0;

		our_aln->mapped = bwa_aln->rid >= 0;
		if (bwa_aln->rid >= 0) { // with coordinate
			our_aln->reverse_strand = bwa_aln->is_rev != 0;
			our_aln->contig = our_ref->contigs[bwa_aln->rid];
			our_aln->pos = bwa_aln->pos + 1;
			our_aln->n_mismatches = bwa_aln->NM;
			/* TODO: convert cigars
			if (bwa_aln->n_cigar) { // aligned
				for (i = 0; i < bwa_aln->n_cigar; ++i) {
					int c = bwa_aln->cigar[i]&0xf;
					if (c == 3 || c == 4) c = which? 4 : 3; // use hard clipping for supplementary alignments
					kputw(bwa_aln->cigar[i]>>4, str); kputc("MIDSH"[c], str);
				}
			} else kputc('*', str); // having a coordinate but unaligned (e.g. when copy_mate is true)
			*/
			// setting NULL is redundant since we did a memset earlier
			our_aln->cigar_ops = NULL;
			our_aln->n_cigar_ops = 0;
			// TODO: convert mismatch string MD
			our_aln->mm_def = NULL;
			our_aln->n_mm_defs = 0;
		}

		// TODO: extra tags
		our_aln->tags = NULL;

		//if (bwa_aln->sub >= 0) { kputsn("\tXS:i:", 6, str); kputw(bwa_aln->sub, str); }

		/* this section outputs other primary hits in the SA tag
		if (!(bwa_aln->flag & 0x100)) { // not multi-hit
			for (i = 0; i < n; ++i)
				if (i != which && !(list[i].flag&0x100)) break;
			if (i < n) { // there are other primary hits; output them
				kputsn("\tSA:Z:", 6, str);
				for (i = 0; i < n; ++i) {
					const mem_aln_t *r = &list[i];
					int k;
					if (i == which || (list[i].flag&0x100)) continue; // proceed if: 1) different from the current; 2) not shadowed multi hit
					kputs(bns->anns[r->rid].name, str); kputc(',', str);
					kputl(r->pos+1, str); kputc(',', str);
					kputc("+-"[r->is_rev], str); kputc(',', str);
					for (k = 0; k < r->n_cigar; ++k) {
						kputw(r->cigar[k]>>4, str); kputc("MIDSH"[r->cigar[k]&0xf], str);
					}
					kputc(',', str); kputw(r->mapq, str);
					kputc(',', str); kputw(r->NM, str);
					kputc(';', str);
				}
			}
		}
		*/
	}
	return ALN_NO_ERROR;
}


/*
 * Based on mem_reg2sam_se.
 * We took out the call to mem_aln2sam and instead write the result to
 * the corresponding aln_read structure.
 */
static int _mem_reg2_rapi_aln_se(const mem_opt_t *opt, aln_read* our_read, const aln_ref* rapi_ref, bseq1_t *seq, mem_alnreg_v *a, int extra_flag, const mem_aln_t *m)
{
	int error = ALN_NO_ERROR;
	const bntseq_t *const bns = (bwaidx_t*)rapi_ref->_private->bns;
	const uint8_t *const pac = (bwaidx_t*)rapi_ref->_private->pac;

	kvec_t(mem_aln_t) aa;
	int k;

	kv_init(aa);
	for (k = 0; k < a->n; ++k) {
		mem_alnreg_t *p = &a->a[k];
		mem_aln_t *q;
		if (p->score < opt->T) continue;
		if (p->secondary >= 0 && !(opt->flag&MEM_F_ALL)) continue;
		if (p->secondary >= 0 && p->score < a->a[p->secondary].score * .5) continue;
		q = kv_pushp(mem_aln_t, aa);
		*q = mem_reg2aln(opt, bns, pac, seq->l_seq, seq->seq, p);
		q->flag |= extra_flag; // flag secondary
		if (p->secondary >= 0) q->sub = -1; // don't output sub-optimal score
		if (k && p->secondary < 0) // if supplementary
			q->flag |= (opt->flag&MEM_F_NO_MULTI)? 0x10000 : 0x800;
		if (k && q->mapq > aa.a[0].mapq) q->mapq = aa.a[0].mapq;
	}
	if (aa.n == 0) { // no alignments good enough; then write an unaligned record
		mem_aln_t t;
		t = mem_reg2aln(opt, bns, pac, seq->l_seq, seq->seq, 0);
		t.flag |= extra_flag;
		// RAPI
		error = _bwa_aln_to_rapi_aln(rapi_ref, our_read, 0, seq, &t, 1);
	}
	else {
		error = _bwa_aln_to_rapi_aln(rapi_ref, our_read, /* unpaired */ 0, seq, /* list of aln */ aa.a, aa.n);
	}

	if (aa.n > 0)
	{
		for (int k = 0; k < aa.n; ++k)
			free(aa.a[k].cigar);
		free(aa.a);
	}
	return error;
}
#endif
/********** end modified BWA code *****************/

int aln_align_reads( const aln_ref* ref,  aln_batch * batch, const aln_opts * config, aln_aligner_state* state )
{
	int error = ALN_NO_ERROR;

	if (batch->n_reads_frag > 2)
		return ALN_OP_NOT_SUPPORTED_ERROR;

	if (batch->n_reads_frag <= 0)
		return ALN_PARAM_ERROR;

	// "extract" BWA-specific structures
	mem_opt_t*const bwa_opt = (mem_opt_t*) config->_private;

	if (batch->n_reads_frag == 2) // paired-end
		bwa_opt->flag |= MEM_F_PE;

	if ((error = _convert_opts(config, bwa_opt)))
		return error;
	fprintf(stderr, "opts converted\n");

	// traslate our read structure into BWA reads
	bwa_batch bwa_seqs;
	if ((error = _batch_to_bwa_seq(batch, config, &bwa_seqs)))
		return error;
	fprintf(stderr, "converted reads to BWA structures.\n");

	_print_bwa_batch(stderr, &bwa_seqs);

	fprintf(stderr, "Going to process.\n");
	mem_alnreg_v *regs = malloc(bwa_seqs.n_reads * sizeof(mem_alnreg_v));
	if (NULL == regs) {
		error = ALN_MEMORY_ERROR;
		goto clean_up;
	}
	fprintf(stderr, "Allocated %d mem_alnreg_v structures\n", bwa_seqs.n_reads);

	extern void kt_for(int n_threads, void (*func)(void*,int,int), void *data, int n);
	bwa_worker_t w;
	w.opt = bwa_opt;
	w.read_batch = &bwa_seqs;
	w.regs = regs;
	w.pes = state->pes;
	w.n_processed = state->n_reads_processed;
	w.rapi_ref = ref;

	fprintf(stderr, "w.opt = %p;          \n" ,  w.opt        );
	fprintf(stderr, "w.read_batch = %p;   \n" ,  w.read_batch );
	fprintf(stderr, "w.regs =  %p;        \n" ,  w.regs       );
	fprintf(stderr, "w.pes = %p;          \n" ,  w.pes        );
	fprintf(stderr, "w.n_processed = %ld;  \n" , w.n_processed);

	fprintf(stderr, "Calling bwa_worker_1. bwa_opt->flag: %d\n", bwa_opt->flag);
	int n_fragments = (bwa_opt->flag & MEM_F_PE) ? bwa_seqs.n_reads / 2 : bwa_seqs.n_reads;
	kt_for(bwa_opt->n_threads, bwa_worker_1, &w, n_fragments); // find mapping positions

	if (bwa_opt->flag & MEM_F_PE) { // infer insert sizes if not provided
		// TODO: support manually setting insert size dist parameters
		// if (pes0) memcpy(pes, pes0, 4 * sizeof(mem_pestat_t)); // if pes0 != NULL, set the insert-size distribution as pes0
		mem_pestat(bwa_opt, ((bwaidx_t*)ref->_private)->bns->l_pac, bwa_seqs.n_reads, regs, w.pes); // infer the insert size distribution from data
	}
	kt_for(bwa_opt->n_threads, bwa_worker_2, &w, n_fragments); // generate alignment

	// run the alignment
	//mem_process_seqs(bwa_opt, bwa_idx->bwt, bwa_idx->bns, bwa_idx->pac, state->n_reads_processed, bwa_seqs.n_reads, bwa_seqs.seqs, &state->pes0);

	state->n_reads_processed += bwa_seqs.n_reads;
	fprintf(stderr, "processed %ld reads\n", state->n_reads_processed);

	// print the SAM generated by BWA
	//for (int i = 0; i < bwa_seqs.n_reads; ++i)
	//	fprintf(stderr, "%s\n", bwa_seqs.seqs[i].sam);

	//mem_alnreg_v bwa_alignment;
	//bwa_alignment = mem_align1(bwa_opt, bwa_idx->bwt, bwa_idx->bns, bwa_idx->pac, ks->seq.l, ks->seq.s); // get all the hits
	// translate BWA's results back into our structure.

clean_up:
	free(regs);
	_free_bwa_batch_contents(&bwa_seqs);

	return error;
}

/******* Generic functions ***********
 * These are generally usable. We should put them in a generic rapi.c file.
 *************************************/

/* Allocate reads */
int aln_alloc_reads( aln_batch * batch, int n_reads_fragment, int n_fragments )
{
	if (n_fragments < 0 || n_reads_fragment < 0)
		return ALN_PARAM_ERROR;

	batch->reads = calloc( n_reads_fragment * n_fragments, sizeof(aln_read) );
	if (NULL == batch->reads)
		return ALN_MEMORY_ERROR;
	batch->n_frags = n_fragments;
	batch->n_reads_frag = n_reads_fragment;
	return ALN_NO_ERROR;
}

int aln_free_reads( aln_batch * batch )
{
	for (int f = 0; f < batch->n_frags; ++f) {
		for (int r = 0; r < batch->n_reads_frag; ++r) {
			aln_read* read = aln_get_read(batch, f, r);
			// the reads use a single chunk of memory for id, seq and quality
			free(read->id);
			for (int a = 0; a < read->n_alignments; ++a) {
				aln_kv_free_list(read->alignments[a].tags);
				free(read->alignments[a].cigar_ops);
				free(read->alignments[a].mm_ops);
			}
			free(read->alignments);
			read->n_alignments = 0;
			// *Don't* free the contig name.  It belongs to the contig structure.
		}
	}

	free(batch->reads);
	memset(batch, 0, sizeof(*batch));

	return ALN_NO_ERROR;
}

int aln_set_read(aln_batch* batch,
			int n_frag, int n_read,
			const char* name, const char* seq, const char* qual,
			int q_offset) {
	int error_code = ALN_NO_ERROR;

	if (n_frag >= batch->n_frags || n_read >= batch->n_reads_frag)
		return ALN_PARAM_ERROR;

	aln_read* read = aln_get_read(batch, n_frag, n_read);
	const int name_len = strlen(name);
	const int seq_len = strlen(seq);
	read->length = seq_len;

	// simplify allocation and error checking by allocating a single buffer
	int buf_size = name_len + 1 + seq_len + 1;
	if (qual)
		buf_size += seq_len + 1;

	read->id = malloc(buf_size);
	if (NULL == read->id) { // failed allocation
		fprintf(stderr, "Unable to allocate memory for sequence\n");
		return ALN_MEMORY_ERROR;
	}

	// copy name
	strcpy(read->id, name);

	// sequence
	read->seq = read->id + name_len + 1;
	strcpy(read->seq, seq);

	// the quality, if we have it, may need to be recoded
	if (NULL == qual)
		read->qual = NULL;
	else {
		read->qual = read->seq + seq_len + 1;
		for (int i = 0; i < seq_len; ++i) {
			read->qual[i] = (int)qual[i] - q_offset + 33; // 33 is the Sanger offset.  BWA expects it this way.
			if (read->qual[i] > 127)
			{ // qual is unsigned, by Sanger base qualities have an allowed range of [0,94], and 94+33=127
				fprintf(stderr, "Invalid base quality score %d\n", read->qual[i]);
				error_code = ALN_PARAM_ERROR;
				goto error;
			}
		}
		read->qual[seq_len] = '\0';
	}

	// trim from the name /[12]$
	int t = name_len;
	if (t > 2 && read->id[t-2] == '/' && (read->id[t-1] == '1' || read->id[t-1] == '2'))
		read->id[t-2] = '\0';

	return ALN_NO_ERROR;

error:
	// In case of error, free any allocated memory and return the error
	free(read->id);
	return error_code;
}

