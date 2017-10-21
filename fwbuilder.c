#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fw_entry {
  struct fw_entry *next;
  int startpos;
  char filename[255];
};

struct fw_entry *filelist;

struct fw_entry *filelist_load(char *fn) {
  FILE * f = fopen(fn, "r");
  struct fw_entry *liststart = NULL;
  struct fw_entry **next = &liststart;
  while(!feof(f)) {
    struct fw_entry *entry = calloc(1, sizeof(struct fw_entry));
    if (fscanf(f, "%x %s\n", &entry->startpos, &entry->filename) == 0) {
      free (entry);
      return liststart;
    }
    *next = entry;
    next = &entry->next;
  }
  return liststart;
}
void filelist_free(struct fw_entry *start) {
  struct fw_entry *ptr = start, *ptr2;
  while(ptr != NULL) {
    ptr2 = ptr;
    ptr = ptr->next;
    free(ptr2);
  }
}
void filelist_print(struct fw_entry *start) {
  struct fw_entry *ptr = start;
  while(ptr != NULL) {
    printf("%06x  %s\n", ptr->startpos, ptr->filename);
    ptr = ptr->next;
  }
}

#define MIN(a,b) ((a>b)?(b):(a))
int file_copy(FILE * src, FILE * dst, int length) {
  char buffer[4096];
  while (length > 0) {
    size_t nread = fread(buffer, 1, MIN(4096,length), src);
    if (nread != MIN(4096,length)) return 3;
    if (fwrite(buffer, MIN(4096,length), 1, dst) != 1) return 4;
    length -= nread;
  }
  return 0;
}
int extract_part(FILE * fw, struct fw_entry *entry, char * target_prefix) {
  char target_fn[255];
  snprintf(target_fn, 255, "%s%s", target_prefix, entry->filename);
  int length = entry->next->startpos - entry->startpos;
  printf("Extracting %d bytes from 0x%06x into %s ...", length, entry->startpos, target_fn);
  FILE * target = fopen(target_fn, "wb");
  if (target == NULL) return 1;
  int err;
  if (err = fseek(fw, entry->startpos, SEEK_SET)) goto fail;
  if (err = file_copy(fw, target, length)) goto fail;
fail:
  fclose(target);
  if (err == 0) printf("ok\n"); else printf("error: %d\n", err);
  return err;
}

void split_firmware(FILE * fw, char * target_prefix) {
  struct fw_entry *ptr = filelist;
  while(ptr != NULL && ptr->next != NULL) { // ignore last entry
    if (strlen(ptr->filename) == 0) {
      printf("skipping %x\n", ptr->startpos);
      continue;
    }
    int err = extract_part(fw, ptr, target_prefix);
    if (err) return;
    ptr = ptr->next;
  }
}

void pack_firmware(FILE * fw, char * src_prefix) {
  struct fw_entry *ptr = filelist;
  char target_fn[255];
  while(ptr != NULL) {
	  fseek(fw, ptr->startpos, SEEK_SET);
    
    if (ptr->next != NULL && strlen(ptr->filename) != 0) {
      int length = ptr->next->startpos - ptr->startpos;
      
      snprintf(target_fn, 255, "%s%s", src_prefix, ptr->filename);
      FILE * src = fopen(target_fn, "rb");
      fseek(src, 0, SEEK_END);
      int filesize = ftell(src);
      fseek(src, 0, SEEK_SET);
      printf("Writing %s to address 0x%06x ...", target_fn, ptr->startpos);
      if (filesize < length) { printf("file smaller, zero-padding\n"); length = filesize; }
      else if (filesize > length) { printf("ERROR: file size %d > length %d\n", filesize, length); return; }
      int err = file_copy(src, fw, length);
      if (err != 0) { printf("error %d\n", err); return; } else printf("ok\n");
    } else {
      printf("Seeking address 0x%06x\n", ptr->startpos);
    }
    ptr = ptr->next;
  }
}

int main(int argc, char **argv) {
  filelist = filelist_load(argv[1]);
  if (argc == 5 && argv[2][0] == 's') {
    // split firmware
    FILE * fw = fopen(argv[3], "rb");
    split_firmware(fw, argv[4]);
  } else if (argc == 5 && argv[2][0] == 'p') {
    // pack firmware
    FILE * fw = fopen(argv[3], "wb");
    pack_firmware(fw, argv[4]);
  } else if (argc == 3 && argv[2][0] == 'v') {
    filelist_print(filelist);
  } else {
    printf("Usage:    %s packlist command firmware.bin target/\n", argv[0]);
    printf("Commands: s(plit)  p(ack)\n");
  }
  filelist_free(filelist);
}


