#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "string.h"
#include "unicode.h"
#include "util.h"

char date[256];

static int parse_section(struct parser *p) {
	str_t *section = str_create();
	uint32_t ch;
	while ((ch = parser_getch(p)) != UTF8_INVALID) {
		if (isdigit(ch)) {
			assert(str_append_ch(section, ch) != -1);
		} else if (ch == ')') {
			if (!section->str) {
				break;
			}
			int sec = strtol(section->str, NULL, 10);
			if (sec < 1 || sec > 9) {
				parser_fatal(p, "Expected section between 1 and 9");
				break;
			}
			str_free(section);
			return sec;
		} else {
			parser_fatal(p, "Expected digit or )");
			break;
		}
	};
	parser_fatal(p, "Expected manual section");
	return -1;
}

static void parse_preamble(struct parser *p) {
	str_t *name = str_create();
	int section = -1;
	uint32_t ch;
	while ((ch = parser_getch(p)) != UTF8_INVALID) {
		if (isalnum(ch)) {
			assert(str_append_ch(name, ch) != -1);
		} else if (ch == '(') {
			section = parse_section(p);
		} else if (ch == '\n') {
			if (name->len == 0) {
				parser_fatal(p, "Expected preamble");
			}
			if (section == -1) {
				parser_fatal(p, "Expected manual section");
			}
			char sec[2] = { '0' + section, 0 };
			roff_macro(p, "TH", name->str, sec, date, NULL);
			break;
		}
	}
	str_free(name);
}

static void parse_format(struct parser *p, enum formatting fmt) {
	char formats[FORMAT_LAST] = {
		[FORMAT_BOLD] = 'B',
		[FORMAT_UNDERLINE] = 'I',
	};
	if (p->flags) {
		if ((p->flags & ~fmt)) {
			parser_fatal(p, "Cannot nest inline formatting.");
		}
		fprintf(p->output, "\\fR");
	} else {
		fprintf(p->output, "\\f%c", formats[fmt]);
	}
	p->flags ^= fmt;
}

static void parse_text(struct parser *p) {
	uint32_t ch;
	int i = 0;
	while ((ch = parser_getch(p)) != UTF8_INVALID) {
		switch (ch) {
		case '\\':
			ch = parser_getch(p);
			if (ch == UTF8_INVALID) {
				parser_fatal(p, "Unexpected EOF");
			} else if (ch == '\\') {
				fprintf(p->output, "\\\\");
			} else {
				utf8_fputch(p->output, ch);
			}
			break;
		case '*':
			parse_format(p, FORMAT_BOLD);
			break;
		case '_':
			parse_format(p, FORMAT_UNDERLINE);
			break;
		case '.':
			if (!i) {
				// Escape . if it's the first character
				fprintf(p->output, "\\&.");
				break;
			}
			/* fallthrough */
		default:
			utf8_fputch(p->output, ch);
			break;
		}
		if (ch == '\n') {
			break;
		}
		++i;
	}
}

static void parse_heading(struct parser *p) {
	uint32_t ch;
	int level = 1;
	while ((ch = parser_getch(p)) != UTF8_INVALID) {
		if (ch == '#') {
			++level;
		} else if (ch == ' ') {
			break;
		} else {
			parser_fatal(p, "Invalid start of heading (probably needs a space)");
		}
	}
	switch (level) {
	case 1:
		fprintf(p->output, ".SH ");
		break;
	case 2:
		fprintf(p->output, ".SS ");
		break;
	default:
		parser_fatal(p, "Only headings up to two levels deep are permitted");
		break;
	}
	while ((ch = parser_getch(p)) != UTF8_INVALID) {
		utf8_fputch(p->output, ch);
		if (ch == '\n') {
			break;
		}
	}
}

static int parse_indent(struct parser *p) {
	int i = 0;
	uint32_t ch;
	while ((ch = parser_getch(p)) == '\t') {
		++i;
	}
	parser_pushch(p, ch);
	return i;
}

static void parse_document(struct parser *p) {
	uint32_t ch;
	int indent = 0;
	do {
		int i = parse_indent(p);
		if (i == indent - 1) {
			roff_macro(p, "RE", NULL);
		} else if (i == indent + 1) {
			roff_macro(p, "RS", "4", NULL);
		} else if (i != indent && ch == '\t') {
			parser_fatal(p, "(De)indented by an amount greater than 1");
		}
		indent = i;

		ch = parser_getch(p);
		if (ch == UTF8_INVALID) {
			break;
		}

		if (indent != 0) {
			// Only text is allowed at this point
			parser_pushch(p, ch);
			parse_text(p);
			continue;
		}

		switch (ch) {
		case '#':
			parse_heading(p);
			break;
		case ' ':
			parser_fatal(p, "Tabs are required for indentation");
			break;
		case '\n':
			roff_macro(p, "P", NULL);
			break;
		default:
			parser_pushch(p, ch);
			parse_text(p);
			break;
		}
	} while (ch != UTF8_INVALID);
}

static void output_scdoc_preamble(struct parser *p) {
	// TODO: Add version here
	fprintf(p->output, ".\\\" Generated by scdoc\n");
	fprintf(p->output, ".\\\" Fix weird qutation marks:\n");
	fprintf(p->output, ".\\\" http://bugs.debian.org/507673\n");
	fprintf(p->output, ".\\\" http://lists.gnu.org/archive/html/groff/2009-02/msg00013.html\n");
	fprintf(p->output, ".ie \\n(.g .ds Aq \\(aq\n");
	fprintf(p->output, ".el       .ds Aq '\n");
	fprintf(p->output, ".\\\" Disable hyphenation:\n");
	roff_macro(p, "nh", NULL);
	fprintf(p->output, ".\\\" Generated content:\n");
}

int main(int argc, char **argv) {
	if (argc > 1) {
		fprintf(stderr, "Usage: scdoc < input.scd > output.roff");
		return 1;
	}
	time_t now;
	time(&now);
	struct tm *now_tm = localtime(&now);
	strftime(date, sizeof(date), "%F", now_tm);
	struct parser p = {
		.input = stdin,
		.output = stdout,
		.line = 1,
		.col = 1
	};
	output_scdoc_preamble(&p);
	parse_preamble(&p);
	parse_document(&p);
	return 0;
}
