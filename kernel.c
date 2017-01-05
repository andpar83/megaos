/* Surely you will remove the processor conditionals and this comment
   appropriately depending on whether or not you use C++. */
#if !defined(__cplusplus)
#include <stdbool.h> /* C doesn't have booleans by default. */
#endif
#include <stddef.h>
#include <stdint.h>
 
/* Check if the compiler thinks we are targeting the wrong operating system. */
#if defined(__linux__)
//#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif


#include "keyboard_map.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

/* Hardware text mode color constants. */
enum vga_color {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
	return fg | bg << 4;
}
 
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
	return (uint16_t) uc | (uint16_t) color << 8;
}
 
size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;
 
void terminal_initialize(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}
 
void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}


void terminal_newline(void)
{
	terminal_column = 0;
	if (++terminal_row == VGA_HEIGHT)
		terminal_row = 0;
}
 
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}
 
void terminal_putchar(char c) {
        if (c == '\n') {
		terminal_newline();
	}
	else {
		terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
		if (++terminal_column == VGA_WIDTH) {
			terminal_column = 0;
			if (++terminal_row == VGA_HEIGHT)
				terminal_row = 0;
		}
	}
}
 
void terminal_write(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}
 
void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

#define ENTER_KEY_CODE 0x1C

extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);

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
			terminal_newline();
			return;
		}

		terminal_putchar(keyboard_map[keycode]);
	}
}

void __attribute__((cdecl))  kmain(int start_eip, int start_esp)
{
	const char *str = "my first kernel\n";
        char addr_str[sizeof(int) * 2 + 3];

	terminal_initialize();
       
	terminal_writestring("Code start address: "); 
        itoa_hex(start_eip, addr_str);
	terminal_writestring(addr_str);
	terminal_writestring("\n");

	terminal_writestring("Stack start address: ");
	itoa_hex(start_esp, addr_str);
	terminal_writestring(addr_str);
	terminal_writestring("\n");

	terminal_writestring(str);

	idt_init();
	kb_init();

	while(1)
	{
		asm volatile ("hlt");
	}
}
