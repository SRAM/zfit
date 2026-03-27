#include "../heatshrink/heatshrink_common.h"
#include "heatshrink_config.h"
#include "../heatshrink/heatshrink_encoder.c"
#include "../fit_delta_encode.c"

int window = 9;
int lookahead = 4;
int log2_buf_size = 9;

int write_hs(struct fit_delta_st *fd, uint8_t *buf, size_t s) {  
  heatshrink_encoder *p_hse = (heatshrink_encoder *)(fd->heatshrink_state);
  
  size_t res=0;
  
  while (s) {
    size_t consumed=0;

    if (heatshrink_encoder_sink(p_hse, buf+res, s, &consumed)) {
      fprintf(stderr,"error\n");
      return -1;
    }
    res += consumed;
    s -= consumed;

    int r;
#define OUT_BUF_SIZE 32
    do {
      uint8_t out_buf[OUT_BUF_SIZE];
      size_t out_size;

      r=heatshrink_encoder_poll(p_hse, out_buf, OUT_BUF_SIZE, &out_size);

      if (r<0) {
	fprintf(stderr, "poll error %d\n",r);
	return r;
      }

      if (out_size != (size_t)fwrite(out_buf, 1, out_size, fd->fd_out)) {
	fprintf(stderr,"error\n");
	return -2; 
      }

    } while (r==HSER_POLL_MORE);
  }
  return res;
}

static enum fit_delta_errors_e write_fit_zip_header(FILE *fd_out) {
  uint8_t data[3];
  data[0]='Z'; 
  data[1]=(window << 4) + \
    (lookahead);
  data[2]=delta_log2_buf_size;

  if (3!=fwrite(data, 1, 3, fd_out)) return FIT_DELTA_ERROR_IO_ERROR;
  return FIT_DELTA_ERROR_NO_ERROR;
}

int main(int argc, char **argv) {

  int i;
  for (i=1; i<argc; i++) {
    char *arg = argv[i];
    if (0==strcmp(arg,"-w")) {
      window = atoi(argv[++i]);
    } else if (0==strcmp(arg,"-l")) {
      lookahead = atoi(argv[++i]);
    } else if (0==strcmp(arg,"-b")) {
      log2_buf_size = atoi(argv[++i]);
    } else {
      fprintf(stderr, "Unknown argument '%s'\n",arg);
      return -1;
    }
  }

  struct fit_delta_st fd;
  fit_delta_init(&fd);
  fit_delta_alloc_buf(&fd, log2_buf_size);

  heatshrink_encoder *p_hse = heatshrink_encoder_alloc(window, lookahead);
  fd.heatshrink_state = p_hse;

  fd.fd_in = stdin;
  fd.fd_out = stdout;

  if (0 != window) {
    fd.write_out = &write_hs;
  }

  enum fit_delta_errors_e ret;

  if (window) {
    ret = write_fit_zip_header(stdout);
    if (ret != FIT_DELTA_ERROR_NO_ERROR) return ret;
  }

  ret = fit_delta_process(&fd);
  if (ret != FIT_DELTA_ERROR_NO_ERROR) return ret;
 
  if (window) {
    while (HSER_FINISH_MORE==heatshrink_encoder_finish(p_hse)) {
      uint8_t buf[32];
      size_t out_size;
      
      heatshrink_encoder_poll(p_hse,
			      buf, 32, &out_size);
      fwrite(buf, 1, out_size, stdout);
    }
  }
  return 0;
}
