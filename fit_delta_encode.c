#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum fit_delta_errors_e {
  FIT_DELTA_ERROR_NO_ERROR = 0,
  FIT_DELTA_ERROR_EOF = -1,
  FIT_DELTA_ERROR_FIT_PARSE = -2,
  FIT_DELTA_ERROR_ZFIT_PARSE = -3,
  FIT_DELTA_ERROR_IO_ERROR = -4,
  FIT_DELTA_ERROR_UNZIP_BUF_TOO_SMALL = -5};

#if 0
#define debug_printf(args...) fprintf(stderr, args)
#else
#define debug_printf(args...) do { ; } while (0)
#endif

struct message_definition_st {
  int length;
  int cmp_length;
  int global_message_number;
  uint8_t *last_data;
};

struct fit_delta_st {
  _Bool decoding; // false if encoding

  int (*read_in)(struct fit_delta_st *this, uint8_t *buf, size_t n);
  int (*write_out)(struct fit_delta_st *this, uint8_t *buf, size_t n);
  FILE *fd_in;
  FILE *fd_out;

  void *heatshrink_state; // for API convenience, points to a
			  // heatshrink_encoder or heatshrink_decoder
			  // in practice.

#if HEATSHRINK_DYNAMIC_ALLOC
  uint8_t *data_buf;
#else
#define DELTA_LOG2_BUF_SIZE (9)
#define DELTA_BUF_SIZE (1<<DELTA_LOG2_BUF_SIZE)  
  uint8_t data_buf[DELTA_BUF_SIZE];
#endif
  uint8_t *data_buf_head;
  struct message_definition_st message_definitions[16];
};

#if HEATSHRINK_DYNAMIC_ALLOC
int delta_log2_buf_size = 9;
int delta_buf_size = 0; // update on malloc

void fit_delta_alloc_buf(struct fit_delta_st *this, int log2_buf_size) {
  delta_log2_buf_size = log2_buf_size;
  delta_buf_size = 1<<delta_log2_buf_size;
  this->data_buf_head = this->data_buf = malloc(delta_buf_size);  
};

#else
static const int delta_buf_size = DELTA_BUF_SIZE;
static const int delta_log2_buf_size = DELTA_LOG2_BUF_SIZE;
#endif


int read_in_default(struct fit_delta_st *this, uint8_t *buf, size_t n) {
  return fread(buf, 1, n, this->fd_in);
}

int write_out_default(struct fit_delta_st *this, uint8_t *buf, size_t n) {
  return fwrite(buf, 1, n, this->fd_out);
}

void fit_delta_init(struct fit_delta_st *this) {
  memset(this, 0, sizeof(*this));

  this->data_buf_head = this->data_buf;

  this->read_in = read_in_default;
  this->write_out = write_out_default;

  int i;
  for (i=0; i<16; i++)
    this->message_definitions[i].global_message_number=-1;
}

static int readthru(struct fit_delta_st *this, uint8_t *buf, int n) {
  int r = this->read_in(this, buf, n);
  return this->write_out(this, buf, r);
}

static enum fit_delta_errors_e parse_file_header(struct fit_delta_st *this) {

  uint8_t data[20];
  readthru(this,data,1);

  if (data[0] > 20) {
    return FIT_DELTA_ERROR_EOF;
  }

  if ((data[0]-1) != readthru(this,data+1,data[0]-1)) 
    return FIT_DELTA_ERROR_EOF;

  if (memcmp(data+8,".FIT",4)) {
    fprintf(stderr, ".FIT missing");
    return FIT_DELTA_ERROR_FIT_PARSE;
  }
  else return FIT_DELTA_ERROR_NO_ERROR;
}

static enum fit_delta_errors_e parse_data_message(struct fit_delta_st *this, int local_type) {
  struct message_definition_st *m=this->message_definitions+local_type;

  uint8_t data[256];

  if (m->global_message_number < 0) {
    debug_printf("bogus message_definition\n");
    debug_printf("ftell %08lx\n", ftell(this->in));
  }

  debug_printf("%c", "cd"[this->decoding]);
  debug_printf("Read message %d->g#%d ", local_type, m->global_message_number);
  
  int readlen=this->read_in(this, data, m->cmp_length);
  
  uint8_t *last_data;

  last_data = m->last_data;
    
  int i;
  for (i=0; i<m->cmp_length; i++) {

    debug_printf("%02x ",data[i]);
    
    uint8_t d;
    if (this->decoding) {
      data[i]+=last_data[i];
      d=data[i];
    } else {
      d=data[i];
      data[i]-=last_data[i];
    }
    last_data[i]=d;
  }
  
  debug_printf("\n");
  
  int writelen=this->write_out(this, data, readlen);
  if (writelen != readlen) return FIT_DELTA_ERROR_IO_ERROR;
  if (readlen != m->cmp_length) return FIT_DELTA_ERROR_EOF;
  
  while (writelen < m->length) {
    
    //fprintf(stderr, "failed compression opportunity %d %d/%d\n",m->global_message_number,m->cmp_length,m->length);
    
    readlen = m->length - writelen;
    
    if (readlen > 256)
      readlen = 256;
    
    if (readthru(this, data,readlen) < readlen)
      return FIT_DELTA_ERROR_EOF;
    writelen += readlen;
  }

  return FIT_DELTA_ERROR_NO_ERROR;
}

static enum fit_delta_errors_e parse_def_message(struct fit_delta_st *this, int local_type) {

  struct message_definition_st *m=this->message_definitions+local_type;

  m->length=0;
  uint8_t arch;
  int ret = readthru(this, &arch,1);
  if (1 != ret) return FIT_DELTA_ERROR_EOF;
  //  if (arch > 1) {
  //  return FIT_DELTA_ERROR_FIT_PARSE;
  //}

  ret = readthru(this, &arch,1);
  if (1 != ret) return FIT_DELTA_ERROR_EOF;

  union {
    struct {
      uint16_t global_message_number;
      uint8_t field_ct;
    };
    uint8_t data[3];
  } d;

  ret = readthru(this,d.data,3);
  if (3 != ret) return FIT_DELTA_ERROR_EOF;

  debug_printf("global message number %d ",d.global_message_number);

  m->global_message_number = d.global_message_number;
  
  int i;
  for (i=0; i<d.field_ct; i++) {
    union {
      struct {
	uint8_t message_number;
	uint8_t size;
	uint8_t base_type;
      };
      uint8_t data[3];
    } f;

    ret = readthru(this,f.data,3);
    if (3 != ret) return FIT_DELTA_ERROR_EOF;
    debug_printf("#%d (%d*%d) ",f.message_number,f.size,f.base_type);
    
    m->length += f.size;
  }

  //  fprintf(stderr, "id %d is %d bytes\n",
  //	  local_type, m->length);

  debug_printf("%d \n",m->length);

  if (m->last_data) {
    // existing data, let's garbage collect before reusing the local slot.

    memmove(m->last_data, 
	    m->last_data + m->cmp_length, 
	    (this->data_buf + delta_buf_size) - (m->last_data));
    
    for (i=0; i<16; i++) {
      if (this->message_definitions[i].last_data > m->last_data) {
	this->message_definitions[i].last_data -= m->cmp_length;
      }
    }
    this->data_buf_head -= m->cmp_length;
  }

  m->cmp_length = m->length;
  // we'll limit the compare memory of any one message type to 256 bytes
  if (m->cmp_length > 256) 
    m->cmp_length=256;

  m->last_data = this->data_buf_head;
  this->data_buf_head += m->cmp_length;

  if (this->data_buf + delta_buf_size < this->data_buf_head) {
    //fprintf(stderr, "oom %d %d\n",(int)(this->data_buf_head-this->data_buf), delta_buf_size);

    this->data_buf_head = this->data_buf + delta_buf_size;
    m->cmp_length = 0;
  }
  //fprintf(stderr, "data buf head: %d\n",(int)(data_buf_head - data_buf));


  memset(m->last_data,0,m->cmp_length);
  return 0;
};

#define COMPRESSED_TIMESTAMP (1<<7)
#define DEFINITION_MESSAGE (1<<6)

static enum fit_delta_errors_e parse_record(struct fit_delta_st *this) {
  uint8_t record_header;
  int ret;
  ret=readthru(this,&record_header,1);
  if (ret != 1) return FIT_DELTA_ERROR_EOF;

  if (record_header & COMPRESSED_TIMESTAMP) {
    int local_message_type=(record_header>>5)&0x3;
    ret=parse_data_message(this,local_message_type);
  } else {
    int local_message_type=record_header & 0x0f;

    if (record_header & DEFINITION_MESSAGE) {
      ret=parse_def_message(this,local_message_type);
    } else {
      ret=parse_data_message(this,local_message_type);
    }
  }

  return ret;
}

enum fit_delta_errors_e fit_delta_process(struct fit_delta_st *this) {

  enum fit_delta_errors_e ret;
  ret = parse_file_header(this);
  if (ret != FIT_DELTA_ERROR_NO_ERROR) return ret;

  while (FIT_DELTA_ERROR_NO_ERROR==(ret=parse_record(this)))
    ;

  // EOF is the expected termination
  if (ret==FIT_DELTA_ERROR_EOF) return FIT_DELTA_ERROR_NO_ERROR;
  // otherwise
  return ret;
}

