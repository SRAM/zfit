#include <assert.h>
#include "../heatshrink/heatshrink_common.h"
#include "heatshrink_config.h"
#include "../heatshrink/heatshrink_decoder.c"

#include "../fit_delta_encode.c"

int window = 9;
int lookahead = 4;

#define INPUT_BUFFER_SIZE (32)

struct decoder_state_st {
  uint8_t in_buf[INPUT_BUFFER_SIZE];
  uint8_t in_buf_head; // where valid data starts
  uint8_t in_buf_tail; // where valid data ends
} dst;

int read_hs(struct fit_delta_st *fd, uint8_t *buf, size_t s) {

  heatshrink_decoder *p_hsd = (heatshrink_decoder *)(fd->heatshrink_state);

  assert(((int)s)>=0);

  size_t res=0;
  
  while (s) {
    size_t written;
    int r = heatshrink_decoder_poll(p_hsd, buf, s, &written);

    s -= written;
    buf += written;    
    res += written;

    if (HSDR_POLL_EMPTY == r) {

      int bytes_read = fread(dst.in_buf+dst.in_buf_tail, 1,
			     INPUT_BUFFER_SIZE-dst.in_buf_tail, fd->fd_in);

      if (bytes_read == 0) {
	// done with reading, need to finish
	int r = heatshrink_decoder_finish(p_hsd);
	if (r != HSDR_FINISH_MORE)
	  return res;
      }

      dst.in_buf_tail += bytes_read;

      size_t consumed;
      if (heatshrink_decoder_sink(p_hsd, dst.in_buf+dst.in_buf_head, 
				  dst.in_buf_tail - dst.in_buf_head, &consumed)) {

	fprintf(stderr,"decode error\n");
	return -1;
      }

      dst.in_buf_head += consumed;

      if (dst.in_buf_head==dst.in_buf_tail)
	dst.in_buf_head = dst.in_buf_tail = 0;
    }
  }
  return res;
}

static enum fit_delta_errors_e read_fit_zip_header(struct fit_delta_st *fd) {  

  uint8_t data[3];
  if (3 != fread(data, 1, 3, fd->fd_in)) 
    return FIT_DELTA_ERROR_EOF;

  if (data[0] != 'Z')
    return FIT_DELTA_ERROR_ZFIT_PARSE;

  window = (data[1] >> 4);
  lookahead = (data[1] & 0xf);

  int log2_buf_size = data[2];

  /* 1 MB is just extravagantly large, if we read such a value it's
     not a legit file. */

  if (log2_buf_size > 20) {
    return FIT_DELTA_ERROR_ZFIT_PARSE;
  }

  fit_delta_alloc_buf(fd, log2_buf_size);

  return FIT_DELTA_ERROR_NO_ERROR;
}

int main(int argc, char **argv) {

  int use_zip = 1;
  int log2_buf_size = 9;
  int i;
  for (i=1; i<argc; i++) {
    if (0==strcmp(argv[i],"-d")) {
      use_zip = 0;
    } else if (0==strcmp(argv[i],"-b")) {
      log2_buf_size = atoi(argv[++i]);
    } else {
      fprintf(stderr, "Unknown argument '%s'\n",argv[i]);
      return -1;
    }
  }

  struct fit_delta_st fd;
  fit_delta_init(&fd);

  fd.heatshrink_state = heatshrink_decoder_alloc(4096, window, lookahead);
  fd.fd_in = stdin;
  fd.fd_out = stdout;

  enum fit_delta_errors_e ret;
  if (use_zip) {
    ret = read_fit_zip_header(&fd);
    if (ret != FIT_DELTA_ERROR_NO_ERROR) return ret;
  } else {
    fit_delta_alloc_buf(&fd, log2_buf_size);    
  }
  
  if (use_zip) {
    fd.read_in = &read_hs;
  }

  fd.decoding = 1;

  ret = fit_delta_process(&fd);
  //fprintf(stderr,"\n\nGot %d from fit_delta_process\n",ret);
  if (ret != FIT_DELTA_ERROR_NO_ERROR) return ret;

  return 0;
}
