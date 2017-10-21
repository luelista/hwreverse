#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>

#define BIT unsigned char
struct pins {
  BIT cs : 1;
  BIT x1 : 1;
  BIT data : 1;
  BIT x2 : 2;
  BIT clk : 1;
  BIT x3 : 2;
};

#define HIGH 1
#define LOW 0

#define POSEDGE 1
#define NEGEDGE 0

// SPI config: 

#define CS_ACTIVE LOW
#define CLK_EDGE POSEDGE
// end spi config

#define CLK_EDGE_OLD ((CLK_EDGE==POSEDGE)?LOW:HIGH)
#define CLK_EDGE_NEW ((CLK_EDGE==POSEDGE)?HIGH:LOW)

struct pins state;
char *databuffer;
size_t databufsize;
ssize_t dataidx, datalen;
int curpos = -1;
FILE * datafile;
int err=0;
int debugflag=0;
int samplerate=16000000;
#define ERR_EOF -1
#define ERR_CS_DISABLED -2


#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')
#define AC_RED     "\x1b[31m"
#define AC_GREEN   "\x1b[32m"
#define AC_YELLOW  "\x1b[33m"
#define AC_BLUE    "\x1b[34m"
#define AC_MAGENTA "\x1b[35m"
#define AC_CYAN    "\x1b[36m"
#define AC_RESET   "\x1b[0m"
void debugstate() {
  double sec = 1.0/samplerate * (double)curpos;
  printf(AC_CYAN "%08x %010.8fsec  cs=%d, clk=%d, data=%d  (" BYTE_TO_BINARY_PATTERN ")" AC_RESET "\n", curpos, sec, state.cs, state.clk, state.data, BYTE_TO_BINARY(*(unsigned char*)&state));
}

int nextstate() {
  if (dataidx == datalen) {
    //size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
    datalen = fread(databuffer, 1, databufsize, datafile) * 1;
    dataidx = 0;
    if (datalen < 1) {
      printf("ERR_EOF\n");
      return ERR_EOF;
    }
  }
  state = *(struct pins*)(databuffer+dataidx);
  dataidx++;
  curpos++;
  return 0;
}

int waitforCS() {
  while (state.cs != CS_ACTIVE) {
    if (err = nextstate()) return err;
  }
  if (debugflag) {printf("waitforCS done  \t");  debugstate();}
}

int waitCLK() {
  struct pins oldstate;
  do {
    oldstate = state;
    if (err = nextstate()) return err;
    if (state.cs != CS_ACTIVE) { err = ERR_CS_DISABLED; return err; }
  } while (!(oldstate.clk == CLK_EDGE_OLD && state.clk == CLK_EDGE_NEW));
  return 0;
}

int readbyte() {
  unsigned char byte = 0;
  for(char i = 0; i<8; i++) {
    if (err = waitCLK()) return err;
    byte = (byte << 1) | state.data;
  }
  if (debugflag) {printf("read a byte %02x  \t", byte);  debugstate();}
  return byte;
}
#define HANDLE_ERR(x) if(x==ERR_EOF){printf("eof\n");break;} else if (x<0) {printf("error %d\n");continue;}
int main(int argc, char **argv) {
  int cmdbyte, adrbyte, datbyte, address, readbytes, verbose = 0;
  int maxlen = 0;
  int c;
  FILE * jsonfile = NULL;
  while ((c = getopt(argc, argv, "dvm:r:J:")) != -1) {
    switch (c) {
    case 'v':
      verbose = 1;
      break;
    case 'm':
      maxlen = atoi(optarg);
      break;
    case 'r':
      samplerate = atoi(optarg);
      break;
    case 'J':
      jsonfile = fopen(optarg, "w");
      break;
    case 'd':
      debugflag = 1;
      break;
    }
  }
  if (optind >= argc) {
    fprintf(stderr, "missing filename argument\n");
    return 1;
  }

  databufsize = 4096;
  databuffer = malloc(databufsize);
  if (databuffer == NULL) perror("malloc");

  datafile = fopen(argv[optind], "rb");

  if (jsonfile != NULL) fprintf(jsonfile, "[{\"source\":\"%s\"}", argv[optind]);
  while(!feof(datafile)) {
    printf("\n");
    if (err != 0 && jsonfile != NULL) fprintf(jsonfile, "\"err\":%d}", err);
    if (jsonfile != NULL) fprintf(jsonfile, ",\n{", cmdbyte);
    if (debugflag) {printf("loop_start  \terr=%d  \t", err);    debugstate();}
    if((err = waitforCS()) < 0) continue;
    if((cmdbyte = readbyte()) < 0) continue;
    printf("cmd=%02X @ ",cmdbyte);debugstate();
    if (jsonfile != NULL) fprintf(jsonfile, "\"cmd\":%d,", cmdbyte);
    switch(cmdbyte) {
    case 0x03: //read data
      address = 0;
      if((adrbyte = readbyte()) < 0) continue;
      address |= adrbyte; address <<= 8;
      if((adrbyte = readbyte()) < 0) continue;
      address |= adrbyte; address <<= 8;
      if((adrbyte = readbyte()) < 0) continue;
      address |= adrbyte;
      printf("READ from addr=" AC_GREEN "0x%06x " AC_RESET , address);
      readbytes = 0;
      if (jsonfile != NULL) fprintf(jsonfile, "\"read_address\":%d,\"data\":\"", address);
      while((datbyte = readbyte()) >= 0) {
        if (readbytes < maxlen) printf("%02x ", datbyte);
        else if (readbytes == maxlen) printf("... ");
        if (jsonfile != NULL) fprintf(jsonfile, "%02x", datbyte);
        readbytes ++;
      }
      printf("len=" AC_YELLOW "%04x" AC_RESET "\n", readbytes);
      if (jsonfile != NULL) fprintf(jsonfile, "\"}", address);
      break;
    case 0x05: case 0x35: //read status reg
      printf("read STATUS reg with cmd=0x%02x ", cmdbyte);
      while((datbyte = readbyte()) >= 0) {
        printf(" 0x%02x  ", datbyte);
      }
      if (jsonfile != NULL) fprintf(jsonfile, "\"status_reg_result\":%d}", datbyte);
      printf("\n");
      break;
    default:
      printf("cmd " AC_RED "%02x not implemented" AC_RESET "\n", cmdbyte);
      if (jsonfile != NULL) fprintf(jsonfile, "\"not_implemented\":%d,\"data\":\"", cmdbyte);
      while((datbyte = readbyte()) >= 0) {
        printf("0x%02x ", datbyte);
        if (jsonfile != NULL) fprintf(jsonfile, "%02x", datbyte);
      }
      if (jsonfile != NULL) fprintf(jsonfile, "\"}", cmdbyte);
      printf("\n");
      break;
    }
    err = 0;


  }
  if (jsonfile != NULL) fprintf(jsonfile, "\"err\":%d}]", err);
}

