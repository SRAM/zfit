#include "../fit_delta_encode.c"
#include "../heatshrink/heatshrink_common.h"
#include "heatshrink_config.h"
#include "../heatshrink/heatshrink_encoder.c"

struct fit_zip_st {
  struct fit_delta_st fd;
  heatshrink_encoder hse;
};

int write_hs(struct fit_delta_st *fd, uint8_t *buf, size_t s) {

  heatshrink_encoder *p_hse = (heatshrink_encoder *)(fd->heatshrink_state);

  size_t written=0;
  
  while (s) {
    size_t consumed=0;

    if (heatshrink_encoder_sink(p_hse, buf+written, s, &consumed)<0) {
      fprintf(stderr,"error\n");
      return -1;
    }
    written += consumed;
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
  return written;
}

static enum fit_delta_errors_e write_fit_zip_header(FILE *out_fd) {
  uint8_t data[3];
  data[0]='Z'; 
  data[1]=(HEATSHRINK_STATIC_WINDOW_BITS << 4) + \
    (HEATSHRINK_STATIC_LOOKAHEAD_BITS);
  data[2]=DELTA_LOG2_BUF_SIZE;
  if (3!=fwrite(data, 1, 3, out_fd)) return FIT_DELTA_ERROR_IO_ERROR;  
  return FIT_DELTA_ERROR_NO_ERROR;
}

int read_stdin(uint8_t *buf, size_t s) {
  return fread(buf, 1, s, stdin);
}

static void fit_zip_init(struct fit_zip_st *this) {  
  fit_delta_init(&this->fd);
  this->fd.heatshrink_state = &this->hse;
  heatshrink_encoder_reset(&this->hse);
}

static enum fit_delta_errors_e fit_zip_process(struct fit_zip_st *this,
					       FILE *in, FILE *out) {

  fit_zip_init(this);
  this->fd.fd_in = in;
  this->fd.fd_out = out;
  this->fd.write_out = &write_hs;

  enum fit_delta_errors_e ret;
  ret = write_fit_zip_header(out);
  if (ret != FIT_DELTA_ERROR_NO_ERROR) return ret;

  ret = fit_delta_process(&this->fd);
  //fprintf(stderr,"\n\nGot %d from fit_delta_process\n",ret);
  if (ret != FIT_DELTA_ERROR_NO_ERROR) return ret;
 
  while (HSER_FINISH_MORE==heatshrink_encoder_finish(&this->hse)) {
    uint8_t buf[32];
    size_t out_size;
    
    heatshrink_encoder_poll(&this->hse,
			    buf, 32, &out_size);
    if (out_size != fwrite(buf, 1, out_size, out)) return FIT_DELTA_ERROR_IO_ERROR;
  }

  return FIT_DELTA_ERROR_NO_ERROR;
}

#ifdef FIT_ZIP_MAIN
int main(int argc, char **argv) {

  (void)argc; // unused
  (void)argv; // unused

  struct fit_zip_st fz;
  return fit_zip_process(&fz, stdin, stdout);
}
#endif
