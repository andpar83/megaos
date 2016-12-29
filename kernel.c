#include "keyboard_map.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

#define ENTER_KEY_CODE 0x1C

extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);

/* current cursor location */
unsigned int current_loc = 0;
/* video memory begins at address 0xb8000 */
char *vidptr = (char*)0xb8000;


void kprint(const char* str);
void kprint_newline(void);


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

struct IDT_entry{
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
} __attribute__((packed));

struct IDT_entry IDT[IDT_SIZE];

struct idt_ptr {
	unsigned short limit;
	unsigned long base;
} __attribute__((packed));

extern void load_idt(struct idt_ptr *idt_ptr);

void idt_init(void)
{
	unsigned long keyboard_address;
	unsigned long idt_address;
	struct idt_ptr idt_ptr;

	/* populate IDT entry of keyboard's interrupt */
	keyboard_address = (unsigned long)keyboard_handler; 
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = 0x08; /* KERNEL_CODE_SEGMENT_OFFSET */
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = 0x8e; /* INTERRUPT_GATE */
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;
	

	/*     Ports
	*	 PIC1	PIC2
	*Command 0x20	0xA0
	*Data	 0x21	0xA1
	*/

	/* ICW1 - begin initialization */
	write_port(0x20 , 0x11);
	write_port(0xA0 , 0x11);

	/* ICW2 - remap offset address of IDT */
	/*
	* In x86 protected mode, we have to remap the PICs beyond 0x20 because
	* Intel have designated the first 32 interrupts as "reserved" for cpu exceptions
	*/
	write_port(0x21 , 0x20);
	write_port(0xA1 , 0x28);

	/* ICW3 - setup cascading */
	write_port(0x21 , 0x00);  
	write_port(0xA1 , 0x00);  

	/* ICW4 - environment info */
	write_port(0x21 , 0x01);
	write_port(0xA1 , 0x01);
	/* Initialization finished */

	/* mask interrupts */
	write_port(0x21 , 0xff);
	write_port(0xA1 , 0xff);

	/* fill the IDT descriptor */
	idt_ptr.limit = sizeof(IDT);
	idt_ptr.base = (unsigned long)IDT;

	load_idt(&idt_ptr);
}

void kb_init(void)
{
	/* 0xFD is 11111101 - enables only IRQ1 (keyboard)*/
	write_port(0x21 , 0xFD);
}

void keyboard_handler_main(void) 
{
	unsigned char status;
	char keycode;

	/* write EOI */
	write_port(0x20, 0x20);

	status = read_port(KEYBOARD_STATUS_PORT);
	/* Lowest bit of status will be set if buffer is not empty */
	if (status & 0x01) {
		keycode = read_port(KEYBOARD_DATA_PORT);
		if(keycode < 0)
			return;

		if(keycode == ENTER_KEY_CODE) {
			kprint_newline();
			return;
		}

		vidptr[current_loc++] = keyboard_map[keycode];
		vidptr[current_loc++] = 0x07;	
	}
}

void kprint(const char *str)
{
	unsigned int i = 0;
	while (str[i] != '\0') {
		vidptr[current_loc++] = str[i++];
		vidptr[current_loc++] = 0x07;
	}
}

void kprint_newline(void)
{
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}

void clear_screen(void)
{
	unsigned int i = 0;
	while (i < SCREENSIZE) {
		vidptr[i++] = ' ';
		vidptr[i++] = 0x07;
	}
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

	idt_init();
	kb_init();

	while(1)
	{
		asm volatile ("hlt");
	}
}
