
void print(const char* s, unsigned int addr, char color)
{
	char *vidptr = (char*)0xb8000; 	//video mem begins here.
        vidptr += addr;
	while (*s)
	{
		/* the character's ascii */
		*vidptr++ = *s++;
		/* attribute-byte: give character bg and fg */
		*vidptr++ = color;
	}
}

char hex(int v)
{
	if (v >= 0 && v < 10)
		return v + '0';
	if (v >= 10 && v < 16)
		return (v - 10) + 'A';
	return '-';
}

void itoa_hex(int v, char* s)
{
	*s++ = '0';
	*s++ = 'x';
	*s++ = hex((v >> 28) & 0xf);
	*s++ = hex((v >> 24) & 0xf);
	*s++ = hex((v >> 20) & 0xf);
	*s++ = hex((v >> 16) & 0xf);
	*s++ = hex((v >> 12) & 0xf);
	*s++ = hex((v >> 8) & 0xf);
	*s++ = hex((v >> 4) & 0xf);
	*s++ = hex((v >> 0) & 0xf);
	*s++ = 0;
}

/*
*  kernel.c
*/
void __attribute__((cdecl))  kmain(int start_eip, int start_esp)
{
	const char *str = "my first kernel";
        char addr_str[sizeof(int) * 2 + 3];
        

	char *vidptr = (char*)0xb8000; 	//video mem begins here.
	unsigned int i = 0;
	unsigned int j = 0;

	/* this loops clears the screen
	* there are 25 lines each of 80 columns; each element takes 2 bytes */
	while(j < 80 * 25 * 2) {
		/* blank character */
		vidptr[j] = ' ';
		/* attribute-byte - light grey on black screen */
		vidptr[j+1] = 0x07; 		
		j = j + 2;
	}

	print(str, 60, 0x07);
        itoa_hex(start_eip, addr_str);
	print(addr_str, 0, 0x07);
	itoa_hex(start_esp, addr_str);
	print(addr_str, 30, 0x07);
}
