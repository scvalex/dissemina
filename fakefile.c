#include <stdio.h>
#include <string.h>

typedef unsigned int uint;

void printFILE(FILE f) {
	printf("FILE:\n");
	printf("\t_flags: %x | _mode: %x\n", f._flags, f._mode);
	printf("\t_flags2: %x\n", f._flags2);

	printf("\t_IO_read_ptr : %8x | _IO_read_end : %8x | _IO_read_base   : %8x\n", (uint)f._IO_read_ptr, (uint)f._IO_read_end, (uint)f._IO_read_base);
	printf("\t_IO_write_ptr: %8x | _IO_write_end: %8x | _IO_write_base  : %8x\n", (uint)f._IO_write_ptr, (uint)f._IO_write_end, (uint)f._IO_write_base);
	printf("\t               %8s | _IO_buf_end  : %8x | _IO_buf_base    : %8x\n", "", (uint)f._IO_buf_end, (uint)f._IO_buf_base);
	printf("\t               %8s | _IO_save_end : %8x | _IO_save_base   : %8x\n", "", (uint)f._IO_save_end, (uint)f._IO_save_base);
	printf("\t               %8s |              : %8s | _IO_backup_base : %8x\n", "", "", (uint)f._IO_save_base);
}

int main(int argc, char *argv[]) {
	printf("This if fakefile\n");

	char vah[128] = "I'll go to hell for this.";
	char buf[100];

	FILE *rf = fopen("listen.cpp", "r");
	fread(buf, 1, 1, rf);
	printFILE(*rf);

	FILE intf;
	memset(&intf, 0, sizeof(intf));
	intf._flags = 0xfbad2488;
	intf._IO_read_ptr = intf._IO_read_base = intf._IO_buf_base = vah;
	intf._IO_write_ptr = intf._IO_write_base = vah;
	intf._IO_read_end = intf._IO_buf_end = intf._IO_write_end = vah + sizeof(vah);
	intf._mode = 0xffffffff;
	FILE *f = &intf;
	printFILE(*f);
	memset(buf, 0, sizeof(buf));
	fread(buf, 1, 4, f);
	printf("%s\n", buf);
	printFILE(*f);
	fread(buf, 1, 100, f);
	printf("%s\n", buf);
	printFILE(*f);

	printf("Moving out of fakefile\n");
	return 0;
}

