/**
 * @File sheet.c
 * @Brief IZP Project 1 - Simple command line editor of tables
 * @Author Tadeas Vintrlik
 * @Date Start: 1 Oct 2020
 * @Date Last Modification: 11 Nov 2020
 * The file was written using hard tabs and intended for swiftwidth of 4 and
 * line limit of 80
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// CONSTANTS
#define NO_COMMANDS 18
#define LENGTH_NAME 11
#define MAX_CELL 100
#define MAX_ROW 10240	// 10KiB in ASCII
#define MAX_USER_ARGS 4
#define ASCII_OFFSET 32
#define NO_CONVERSION 0
#define EMPTY_COL -2
#define MOD_START 0	// First index of mod commands
#define MOD_END 7	// Last index of mod commands
#define DATA_START 8	// First index of data commands
#define DATA_END 15	// Last index of data commands
#define SELECTION_START 16	// First index of selection commands
#define SELECTION_END 18	// Last index of selection commands
#define TOO_LONG -1
#define NOT_FOUND -1

// MACROS
#define IS_MOD(CMD_NUM) (CMD_NUM >= MOD_START && CMD_NUM <= MOD_END)
#define IS_DATA(CMD_NUM) (CMD_NUM >= DATA_START && CMD_NUM <= DATA_END)
#define IS_SELECTION(CMD_NUM) (CMD_NUM >= SELECTION_START &&\
		CMD_NUM <= SELECTION_END)
#define ABS(num) (num < 0 ? -(num) : num)

struct command_t
{
	char *name;
	int no_args;
} const commands_s[] = {
	{"irow", 1},	{"arow", 0},	{"drow", 1},	{"drows", 2},	{"icol", 1},
	{"acol", 0},	{"dcol", 1},	{"dcols", 2},	{"cset", 2},	{"tolower", 1},
	{"toupper", 1},	{"round", 1},	{"int", 1},	{"copy", 2},	{"swap", 2},
	{"move", 2},	{"rows", 2},	{"beginswith", 2},	{"contains", 2}
};

typedef struct user_args
{
	int cmd_num;
	int num_args[MAX_USER_ARGS];
	char str_arg[MAX_CELL];
	int dash1;	// if first arg to rows is -
	int dash2;	// if second arg to rows is -
} user_args_t;

typedef struct cmd_types
{
	int mod;	// How many mod commands were called
	int data;	// How many data commands were called
	int selection;	// How many selection commands were called
} cmd_types_t;

enum commands
{
	IROW, AROW, DROW, DROWS, ICOL, ACOL, DCOL, DCOLS, CSET, TOLOWER, TOUPPER,
	ROUND, INT, COPY, SWAP, MOVE, ROWS, BEGINSWITH, CONTAINS
};

/**
 * Return 1 input is a delim 0 otherwise
 * @param char input - char from stdin
 * @param char *delim - string of delim characters
 * @return int - int 1 if delim 0 otherwise
 */
int is_delim(char input, char *delim)
{
	while (*delim)
		if ((*delim++) == input)
			return 1;
	return 0;
}

/**
 * Return 1 if is a command, 0 otherwise
 * @param char* name - parameter from shell
 * @param user_args_t *user_args - struct of user arguments
 * @return int - int 1 if command 0 otherwise
 */
int is_command(char *name, user_args_t *user_args)
{
	for (int i = 0; i <= NO_COMMANDS; i++)
	{
		if (strcmp(commands_s[i].name, name) == 0)
		{
			user_args->cmd_num = i;
			return 1;
		}
	}
	return 0;
}

/**
 * Return number of columns in a row
 * @param char row[MAX_ROW] - row from stdin
 * @param char *delim - string of delim characters
 * @return int - number of columns
 */
int get_no_cols(char row[MAX_ROW], char *delim)
{
	int i, no_delims = 0;
	for (i = 0; i < MAX_ROW && row[i] != '\n'; i++)
	{
		if (is_delim(row[i], delim))
			no_delims++;
	}

	// Number of columns is number of delims + 1
	return no_delims + 1;
}

/**
 * Create an empty row string in char *row with matching amount of columns
 * and a correct delimiter
 * @param char * row - where to create the new empty row
 * @param int no_cols - amount of columns the table row should have
 * @param char *delim - what to use as delimiter
 */
void create_empty_row(char *row, int no_cols, char *delim)
{
	// There will be one less delimiter then there are columns
	while (--no_cols > 0)
		*row++ = *delim; // use first delimiter
	*row++ = '\n';
	*row = '\0';
}

/**
 * Find index of first char column with given number
 * @param char row[MAX_ROW] - row from stdin
 * @param char *delim - delimiter by which to split columns
 * @param int column_number - the desired column to find
 * @return int - index first char of column with number column_number
 * NOT_FOUND if not found
 */
int find_column_start(char row[MAX_ROW], char *delim, int col_desired)
{
	int i, col_current = 1;
	for (i = 0; i < MAX_ROW - 1 && row[i] != '\0'; i++)
	{
		if (col_current == col_desired)
			return i;
		else if (is_delim(row[i], delim))
			col_current++;
	}
	return NOT_FOUND;
}

/**
 * Find index of column end
 * @param char row[MAX_ROW] - row from stdin
 * @param char *delim - delimiter by which to split columns
 * @param int start - where to start the search
 * @param int skip - how many delimiters to skip
 * used if measuring span of more than one column
 */
int find_column_end(char row[MAX_ROW], char *delim, int start, int skip)
{
	for (int i = start; i < MAX_ROW - 1 || row[i] != '\0'; i++)
	{
		/* Column can either end with a delimiter or a newline
		 * Until the desired amount of columns has been skipped continue
		 * Works thanks to short circuit boolean logic in C
		 */
		if ((row[i] == '\n' || is_delim(row[i], delim)) && --skip <= 0)
		{
			if (i == start)
				return EMPTY_COL;
			else
				return i - 1;
		}
	}
	return NOT_FOUND;
}

/**
 * Move all characters in row by offset to the right
 * Fill characters left by the shift with whitespaces
 * @param char row[MAX_ROW] - row from stdin
 * @param int start - first index of characters to move
 * @param offset - amount of characters by which to shift
 * @return int - int 1 if suceeded 0 otherwise
 */
int row_shift_right(char row[MAX_ROW], int start, int offset)
{
	int i = strlen(row);
	int j = i + 1;
	start--; // To move the entire string it must terminate at start - 1
	if (i + offset > MAX_ROW)
	{
		fprintf(stderr, "Line limit exceeded.\n");
		return 0;
	}
	/* Fill out the place after the string with whitespaces
	   This is only neccessary if offset is larger than the
	   distance between start and i, since some chars will
	   be undefined*/
	if (i - start < offset)
	{
		for (; j < i + offset; j++)
			row[j] = ' ';
	}
	for (; i > start; i--)
	{
		row[i + offset] = row[i];
		row[i] = ' ';
	}
	return 1;
}

/**
 * Move all characters in row by offset to the left
 * This will likely delete some chars
 * @param char row[MAX_ROW] - row from stdin
 * @param int start - first index of characters to move
 * @param offset - amount of characters by which to shift
 */
void row_shift_left(char row[MAX_ROW], int start, int offset)
{
	int i, end = strlen(row) + 1;
	for (i = start; i < end; i++)
		row[i] = row[i + offset + 1];
}

/**
 * Replace column number target in row with strcmp
 * @param char *row - row from stdin
 * @param int col_start - first index of the column
 * @param int col_end - last index of the column
 * @param char *str - what to replace the row with
 */
int replace_column(char *row, int col_start, int col_end, char *str)
{
	int col_size, str_size = strlen(str);

	if (col_end == EMPTY_COL)
	{
		col_size = 0;
		col_end = col_start;
	}
	else
		col_size = col_end - col_start + 1;

	if (str_size > col_size)
	{
		if(!row_shift_right(row, col_end, str_size - col_size))
			return 0;
	}
	else if (str_size < col_size)
		row_shift_left(row, col_start, col_size - str_size - 1);

	while (*str)
		row[col_start++] = *str++;
	return 1;
}

/**
 * Return lower-case of c
 * @param char c - character to convert
 * @return char - lower-case of c
 */
char to_lower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c + ASCII_OFFSET;
	else
		return c;
}

/**
 * Return upper-case of c
 * @param char c - character to convert
 * @return char - upper-case of c
 */
char to_upper(char c)
{
	if (c >= 'a' && c <= 'z')
		return c - ASCII_OFFSET;
	else
		return c;
}

/**
 * round number
 * @param double number - number to round down
 * @return int - rounded down number
 */
int my_round(double number)
{
	double after_point = ABS(number) - ABS((int)number);
	if (after_point >= 0.5)
	{
		if (number > 0)
			return (int)number + 1;
		else
			return (int) number - 1;
	}
	else
		return (int)number;
}

/**
 *@param int col_start - first index of the column
 *@param int col_end - last index of the column
 *@param char *str - array into which to store the column
 *@param int conversion number of the type of conversion to do 0 is none
 * The choice to use conversion right when loading cell content was made
 * for the sake of computational difficulty
 */
void get_column_content(char *row, int col_start, int col_end, char *str,
		int conversion)
{
	if (col_end == EMPTY_COL)
	{
		return;
	}
	for (; col_start < col_end + 1; col_start++)
	{
		switch (conversion)
		{
			case NO_CONVERSION:
			*str++ = row[col_start];
			break;
		case TOLOWER:
			*str++ = to_lower(row[col_start]);
			break;
		case TOUPPER:
			*str++ = to_upper(row[col_start]);
			break;
		}
	}
}

void irow_f(int no_cols, char *delim)
{
	char temp[MAX_ROW];
	create_empty_row(temp, no_cols, delim);
	printf("%s", temp);
}

void arow_f(int no_cols, char *delim)
{
	char temp[MAX_ROW];
	create_empty_row(temp, no_cols, delim);
	printf("%s", temp);
}

void drow_f(char *row)
{
	// In most cases it could be enough to just replace the first char with null
	// however if any column shift occur it could make it back into a non empty
	// string again and it is better to handle it this way than to branch in
	// handle_command
	while (*row)
		*row++ = '\0';
}


// functions that represent commands given by arugments are intentionally left
// without doxygen documentation for the sake of file length and readability

int icol_f(char *row, int target, char *delim)
{
	int start;
	if (strlen(row) == MAX_ROW - 1)
	{
		fprintf(stderr, "Line limit exceeded.\n");
		return 0;
	}
	/* To insert a column we first need to find find the start of
	 * the target column and the shift right by one char to
	 * insert the delimeter
	 */
	start = find_column_start(row, delim, target);
	if (!row_shift_right(row, start, 1))
		return 0;
	row[start] = delim[0];
	return 1;
}

int acol_f(char *row, char *delim)
{
	if (strlen(row) == MAX_ROW - 1)
	{
		fprintf(stderr, "Line limit exceeded.\n");
		return 0;
	}
	int length = strlen(row);
	row[length - 1] = delim[0];
	row[length] = '\n';
	row[length + 1] = '\0';
	return 1;
}

void dcols_f(char *row, int target_min, int target_max, char *delim)
{
	int index_start; // index of first char of target_min column
	int index_end;	 // index of last char of target_max column
	int skip;		 // ammount of columns to skip
	index_start = find_column_start(row, delim, target_min);
	skip = target_max - target_min + 1;
	index_end = find_column_end(row, delim, index_start, skip);

	if (target_min == 1 && index_end != EMPTY_COL)
		index_end++; // if first column remove delimiter after
	else
		index_start--; // Remove delimiter before

	if (index_end == EMPTY_COL)
		index_end = index_start; // column is empty
	if (target_min == 1 && target_max == get_no_cols(row, delim))
		index_end--;

	row_shift_left(row, index_start, index_end - index_start);
}

void dcol_f(char *row, int target, char *delim)
{
	dcols_f(row, target, target, delim);
}

int cset_f(char *row, int target, char *str, char *delim)
{
	if (strlen(row) + strlen(str) >= MAX_ROW - 1)
	{
		fprintf(stderr, "Line limit exceeded.\n");
		return 0;
	}
	int col_start = find_column_start(row, delim, target);
	int col_end = find_column_end(row, delim, col_start, 1);
	if (!replace_column(row, col_start, col_end, str))
		return 0;
	return 1;
}

// This function handles tolower and to upper, since most of their code
// would be similar; case_type can be NO_COVERSION, TOUPPER or TOLOWER
void changecase_f(char *row, int target, char *delim, int case_type)
{
	char str[MAX_CELL] = "";
	int col_start = find_column_start(row, delim, target);
	int col_end = find_column_end(row, delim, col_start, 1);
	get_column_content(row, col_start, col_end, str, case_type);
	// replace_column cannot fail here since it only changes case and does not
	// add any additional characters
	replace_column(row, col_start, col_end, str);
}

void tolower_f(char *row, int target, char *delim)
{
	changecase_f(row, target, delim, TOLOWER);
}

void toupper_f(char *row, int target, char *delim)
{
	changecase_f(row, target, delim, TOUPPER);
}

// This handles int and round, since most of their code would be similar
int rounding_f(char *row, int target, char *delim, int round_type)
{
	char *endptr; // string for the rest of strtof
	char cell[MAX_CELL] = "", new_cell[MAX_CELL] = "";
	int start = find_column_start(row, delim, target);
	int end = find_column_end(row, delim, start, 1);
	if (end != EMPTY_COL)
	{
		get_column_content(row, start, end, cell, NO_CONVERSION);
		double to_round = strtod(cell, &endptr);
		if(*endptr)
		{
			fprintf(stderr, "Column contains other data than numbers!\n");
			return 0;
		}
		if (round_type == ROUND)
			sprintf(new_cell, "%d", my_round(to_round));
		else
			sprintf(new_cell, "%d", (int)to_round);
		// replace_column cannot fail here since the rounded down cell
		// will always be shorter
		replace_column(row, start, end, new_cell);
	}
	return 1;
}

int round_f(char *row, int target, char *delim)
{
	return rounding_f(row, target, delim, ROUND);
}

int int_f(char *row, int target, char *delim)
{
	return rounding_f(row, target, delim, INT);
}

int copy_f(char *row, int target_from, int target_to, char *delim)
{
	int col_start_from = find_column_start(row, delim, target_from);
	int col_end_from = find_column_end(row, delim, col_start_from, 1);
	int col_start_to = find_column_start(row, delim, target_to);
	int col_end_to = find_column_end(row, delim, col_start_to, 1);
	char from[MAX_CELL] = "";
	get_column_content(row, col_start_from, col_end_from,from, NO_CONVERSION);
	if (!replace_column(row, col_start_to, col_end_to, from))
		return 0;
	return 1;
}

void swap_f(char *row, int target_from, int target_to, char *delim)
{
	int col_start_from = find_column_start(row, delim, target_from);
	int col_end_from = find_column_end(row, delim, col_start_from, 1);
	int col_start_to = find_column_start(row, delim, target_to);
	int col_end_to = find_column_end(row, delim, col_start_to, 1);
	char from[MAX_CELL] = "";
	char to[MAX_CELL] = "";
	
	get_column_content(row, col_start_from, col_end_from, from, NO_CONVERSION);
	get_column_content(row, col_start_to, col_end_to, to, NO_CONVERSION);
	// replace_column cannot fail here since swaping columns does not
	// add any characters
	replace_column(row, col_start_to, col_end_to, from);

	// Need to find it again, since it might have moved
	col_start_from = find_column_start(row, delim, target_from);
	col_end_from = find_column_end(row, delim, col_start_from, 1);
	replace_column(row, col_start_from, col_end_from, to);
}

int move_f(char *row, int target_from, int target_to, char *delim)
{
	char from[MAX_CELL] = "";
	int col_start_from = find_column_start(row, delim, target_from);
	int col_end_from = find_column_end(row, delim, col_start_from, 1);
	get_column_content(row, col_start_from, col_end_from, from, NO_CONVERSION);
	// Icol can fail if the row is too long
	if (!icol_f(row, target_to, delim))
		return 0;
	// cset can fail if row + from is too long
	if (!cset_f(row, target_to, from, delim))
		return 0;
	// if target was moved from the back to the front an extra column will exist
	// there we must delete one after it, otherwise just delete target_from
	if (target_from > target_to)
		dcol_f(row, target_from + 1, delim);
	else
		dcol_f(row, target_from, delim);
	return 1;
}


/**
 * Return if rows should be selected or no
 */
int rows_f(int n_row, int row_from, int row_to, int is_last_line, int dash1,
		int dash2)
{
	if (dash1 == 1)
		row_from = 0;
	if (dash2 == 1)
		row_to = n_row;
	if (dash1 && dash2)
	{
		if (is_last_line)
			return 1;
		else
			return 0;
	}

	if (n_row >= row_from && n_row <= row_to)
		return 1;
	else
		return 0;
}

int beginswith_f(char *row, int target, char *str, char *delim)
{
	char cell[MAX_CELL];
	int start = find_column_start(row, delim, target);
	int end = find_column_end(row, delim, start, 1);
	get_column_content(row, start, end, cell, NO_CONVERSION);
	int length = strlen(str);

	if (length > (int) strlen(cell))
			return 0;
	for (int i = 0; i < length; i++)
		if (cell[i] != str[i])
			return 0;
	return 1;
}

int contains_f(char *row, int target, char *str, char *delim)
{
	char cell[MAX_CELL];
	int start = find_column_start(row, delim, target);
	int end = find_column_end(row, delim, start, 1);
	get_column_content(row, start, end, cell, NO_CONVERSION);
	int cell_length = strlen(cell);
	int str_length = strlen(str);

	int matches = 0;
	for (int i = 0; i < cell_length; i++)
	{
		if (cell[i] == str[matches])
			matches++;
		else
			matches = 0;

		if (matches == str_length)
			return 1;
	}

	return 0;
}

/**
 * Check if n_row is a valid row argument
 * @param int n_row - argument to check
 * @return int - 1 if is a valid row argument, 0 otherwise
 */
int row_arg_check(int n_row)
{
	if (n_row <= 0)
	{
		fprintf(stderr, "Invalid row number: %d!\n", n_row);
		return 0;
	}
	return 1;
}

/**
 * Check if target is a valid column argument
 * @param int target - argument to check
 * @param int no_cols - number of columns
 * @return int - 1 if is a valid column argument, 0 otherwise
 */
int col_arg_check(int target, int no_cols)
{
	if (target <= 0)
	{
		fprintf(stderr, "Invalid column number: %d!\n", target);
		return 0;
	}
	else if (target > no_cols)
	{
		fprintf(stderr, "Invalid column number: %d!\n", target);
		return 0;
	}
	return 1;
}

/*
 * Check if arg1 is smaller or equal to arg2
 * @param int arg1 - first argument
 * @param int arg2 - second argument
 * @return int - 1 if arg 1 is smaller or equal, 0 otherwise
 */
int two_arg_check(int arg1, int arg2)
{
	if (arg1 > arg2)
	{
		fprintf(stderr, "Invalid arguments: %d !<= %d\n", arg1, arg2);
		return 0;
	}
	return 1;
}

/*
 * Load a line from standard input and replace all delim characters
 * @param char *row - where to store the row from stdin
 * @param char *delim - delim to find and replace
 * @return int - length of loaded row, TOO_LONG if it exceeds MAX_ROW
 */
int load_line(char *row, char *delim)
{
	int i;
	char c;
	for(i = 0; i < MAX_ROW - 1 && (c = getchar())!= EOF; i++)
	{
		// replace delim if delim
		if(is_delim(c, delim))
			*row++ = delim[0];
		else
			*row++ = c;

		// Works thanks to short circuit boolean logic in C
		if ( i == MAX_ROW - 2 && getchar() != EOF)
			return TOO_LONG;

		if (c == '\n')
			break;
	}
	*row++ = '\0';
	if (c == EOF)
		return 0;
	else
		return i + 1; // if line is empty it would return 0, need > 1 for conditions
}


/**
 * Get number of columns after all of the commands changing number of columns
 * @param int no_cols - number of columns before all commands
 * @param user_args_t *user_args - array of all user arguments
 * @param int arg_no - length of user_args array
 * @return int - number of cols adjusted after all
 * the icol, acol, dcol and dcols
 */
int no_cols_adjust(int no_cols, user_args_t *user_args, int arg_no)
{
	for (int i = 0; i < arg_no; i++)
	{
		int n_arg1 = user_args[i].num_args[0];
		int n_arg2 = user_args[i].num_args[1];
		switch(user_args[i].cmd_num)
		{
			case ICOL:
				no_cols++;
				break;
			case ACOL:
				no_cols++;
				break;
			case DCOL:
				no_cols--;
				break;
			case DCOLS:
				no_cols -= n_arg2 - n_arg1 + 1;
				break;
		}
	}
	return no_cols;
}

/*
 * Check for all the things that can go wrong while processing stdin
 * @param char *row - row from stdin
 * @param char *delim - what to use as delimiter
 * @param int prev_cols - number of columns of previous row
 * @return int - 1 if succeeded, 0 if any error encountered
 */
int process_error_handling(char *row, char *delim, int prev_cols, int line_ret)
{
	if (line_ret == TOO_LONG)
	{
		fprintf(stderr, "Line was too long!\n");
		return 0;
	}

	if(get_no_cols(row, delim) != prev_cols)
	{
		fprintf(stderr, "Invalid table!\nDifferent amount of columns\n");
		return 0;
	}
	return 1;
}

/**
 * Call correct commands for table modifications
 * @param user_args_t *user_args - array of structs with called commands
 * @param char *delim - what to use as a delimiter
 * @return int - 1 if success, 0 if error
 */
int process_mod_commands(user_args_t *user_args, int arg_i, char *delim)
{
	char row[MAX_ROW];
	int n_row = 0, init = 0;
	int no_cols = 0;
	int no_cols_adjusted = 0;
	int line_ret;
	while ((line_ret = load_line(row, delim)))
	{
		n_row++;
		if (!init)
		{
			no_cols = get_no_cols(row, delim);
			no_cols_adjusted = no_cols_adjust(no_cols, user_args, arg_i);
			init = 1;
		}
		if(!process_error_handling(row, delim, no_cols, line_ret))
			return 0;
		for (int i = 0; i < arg_i; i++)
		{
			int cmd_num = user_args[i].cmd_num;
			int n_arg1 = user_args[i].num_args[0];
			int n_arg2 = user_args[i].num_args[1];
			switch (cmd_num)
			{
				case IROW:
					if (!row_arg_check(n_arg1))
						return 0;
					if (n_arg1 == n_row)
						irow_f(no_cols_adjusted, delim);
					break;
				case DROW:
					if (!row_arg_check(n_arg1))
						return 0;
					if (n_arg1 == n_row)
						drow_f(row);
					break;
				case DROWS:
					if (!(row_arg_check(n_arg1) && row_arg_check(n_arg2)))
						return 0;
					if (!two_arg_check(n_arg1, n_arg2))
						return 0;
					if (n_arg1 <= n_row && n_arg2 >= n_row)
						drow_f(row);
					break;
				case ICOL:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					if (!icol_f(row, n_arg1, delim))
						return 0;
					break;
				case ACOL:
					if (!acol_f(row, delim))
						return 0;
					break;
				case DCOL:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					dcol_f(row, n_arg1, delim);
					break;
				case DCOLS:
					if (!(col_arg_check(n_arg1, no_cols) &&
						col_arg_check(n_arg2, no_cols)))
						return 0;
					if (!two_arg_check(n_arg1, n_arg2))
						return 0;
					dcols_f(row, n_arg1, n_arg2, delim);
					break;
			}
		}
		printf("%s",row);
	}

	// Handling AROW must happen after the end of stdin
	for (int i = 0; i < arg_i; i++)
		if (user_args[i].cmd_num == AROW)
			arow_f(no_cols_adjusted, delim);
	return 1;
}

/**
 * Check if arguments for the rows command are valid
 * @param int arg1 - first line to select
 * @param int arg2 - last line to select
 * @param int dash1 - if to select from the beginning
 * @param int dash2 - if to select until the end
 * @return int - 1 if everything succeeded, 0 if error encountered
 */
int arg_check_rows(int arg1, int arg2, int dash1, int dash2)
{
	if (arg1 < 1 && !dash1)
	{
		fprintf(stderr, "Invalid row number: %d!\n", arg1);
		return 0;
	}
	if (arg2 < 1 && !dash2)
	{
		fprintf(stderr, "Invalid row number: %d!\n", arg2);
		return 0;
	}
	if (!(dash1 || dash2) && arg1 > arg2)
	{
		fprintf(stderr, "Invalid arguments: %d !<= %d\n", arg1, arg2);
		return 0;
	}
	return 1;
}

/**
 * Call correct commands for data manipulation
 * @param user_args_t *user_args - array of structs with called commands
 * @param char *delim - what to use as a delimiter
 * @return int - 1 if success, 0 if error
 */
int process_data_commands(user_args_t *user_args, int arg_i, char *delim)
{
	char row[MAX_ROW];
	char next[MAX_ROW];
	int n_row = 0, init = 0;
	int no_cols = 0;
	int selected, last_line = 0;
	int line_ret = load_line(next, delim);
	while (line_ret)
	{
		strcpy(row, next);
		line_ret = load_line(next, delim);
		// When last line
		if (!line_ret)
			last_line = 1;
		n_row++;
		if (!init)
		{
			no_cols = get_no_cols(row, delim);
			init = 1;
		}
		if (!process_error_handling(row, delim, no_cols, line_ret))
			return 0;
		selected = 1;
		for (int i = 0; i < arg_i; i++)
		{
			int cmd_num = user_args[i].cmd_num;
			int n_arg1 = user_args[i].num_args[0];
			int n_arg2 = user_args[i].num_args[1];
			char *str = user_args[i].str_arg;
			int dash1 = user_args[i].dash1;
			int dash2 = user_args[i].dash2;
			switch(cmd_num)
			{
				case CSET:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					if(!selected)
						break;
					if (!cset_f(row, n_arg1, str, delim))
						return 0;
					break;
				case TOLOWER:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					if(!selected)
						break;
					tolower_f(row, n_arg1, delim);
					break;
				case TOUPPER:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					if(!selected)
						break;
					toupper_f(row, n_arg1, delim);
					break;
				case ROUND:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					if(!selected)
						break;
					if (!round_f(row, n_arg1, delim))
						return 0;
					break;
				case INT:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					if (!selected)
						break;
					if (!int_f(row, n_arg1, delim))
						return 0;
					break;
				case COPY:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					if (!col_arg_check(n_arg2, no_cols))
						return 0;
					if(!selected)
						break;
					if (!copy_f(row, n_arg1, n_arg2, delim))
						return 0;
					break;
				case SWAP:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					if (!col_arg_check(n_arg2, no_cols))
						return 0;
					if(!selected)
						break;
					swap_f(row, n_arg1, n_arg2, delim);
					break;
				case MOVE:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					if (!col_arg_check(n_arg2, no_cols))
						return 0;
					if(!selected)
						break;
					if (!move_f(row, n_arg1, n_arg2, delim))
						return 0;
					break;
				case ROWS:
					if (!arg_check_rows(n_arg1, n_arg2, dash1, dash2))
						return 0;
					selected = rows_f(n_row, n_arg1, n_arg2, 
							last_line, dash1, dash2);
					break;
				case BEGINSWITH:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					selected = beginswith_f(row, n_arg1, str, delim);
					break;
				case CONTAINS:
					if (!col_arg_check(n_arg1, no_cols))
						return 0;
					selected = contains_f(row, n_arg1, str, delim);
					break;
			}
		}
		printf("%s",row);
	}
	return 1;
}

/**
 * Just print stdin to stdout unless there was an error
 */
int handle_no_commands(char *delim)
{
	char row[MAX_ROW];
	int line_ret, no_cols, init = 0;
	while ((line_ret = load_line(row, delim)))
	{
		if (!init)
		{
			no_cols = get_no_cols(row, delim);
			init = 1;
		}
		if (!process_error_handling(row, delim, no_cols, line_ret))
			return 0;
		printf("%s", row);
	}
	return 1;
}

/*
 * Choose correct proccess function to call
 * @param cmd_types_t cmd_types - struct with ammount and types of commands
 * @param user_args_t *user_args - array of structs with user arguments
 * @param int arg_no - length of user_args array
 * @param char *delim - what to use as delimiter
 * @return int - 1 if everything succeeded, 0 if error encountered
 */
int handle_commands(cmd_types_t cmd_types, user_args_t *user_args,
		int arg_no, char *delim)
{
	if (cmd_types.mod && (cmd_types.data || cmd_types.selection))
	{
		fprintf(stderr, "Unexpected combination of commands!\n");
		return 0;
	}
	else if (cmd_types.mod)
	{
		if (!process_mod_commands(user_args, arg_no, delim))
			return 0;
	}
	else if (cmd_types.data || cmd_types.selection)
	{
		// Selection must always come before data commands otherwise
		// it does not work, this was specified in the forums however
		// if more selections are called it uses a union of those
		if (!process_data_commands(user_args, arg_no, delim))
			return 0;
	}
	else if (!handle_no_commands(delim))
		return 0;

	return 1;
}


/**
 * Check if it is a valid string argument
 * valid string arguments are either arg #2 for cset 
 * or - for rows or arg #2 for beginswith
 * @param char *argv - arugment to check
 * @param int cmd_num - number of command called
 * @param int index - index of num_arg inside the user_args_t struct
 * @param user_args_t *users_args - array of user arguments
 * @return 1 if it is a valid string arugment, 0 otherwise
 * set dash1 or dash2 if called with rows
 */
int valid_str_arg(char *argv, int cmd_num, int index,
		user_args_t *user_args)
{
	if(cmd_num == CSET && index == 1)
		return 1;
	if(cmd_num == BEGINSWITH && index == 1)
		return 1;
	if(cmd_num == CONTAINS && index == 1)
		return 1;
	if(cmd_num == ROWS && (strcmp("-", argv) == 0))
	{
		if (index == 0)
			user_args->dash1 = 1;
		if (index == 1)
			user_args->dash2 = 1;
		return 1;
	}
	return 0;
}

/**
 * Check if it is a valid number argument
 * @param char *argv - arugment to check
 * @param int index - index of num_arg inside the user_args_t struct
 * @param user_args_t *users_args - array of user arguments
 * @return 1 if it is a valid number arugment, 0 otherwise
 */
int valid_num_arg(char *argv, int index, user_args_t *user_args)
{
	char *endptr, function_name[LENGTH_NAME];
	int cmd_num = user_args->cmd_num;
	user_args->num_args[index] = strtol(argv, &endptr, 10);
	if (*endptr)
	{
		strcpy(function_name, commands_s[cmd_num].name);
		fprintf(
			stderr,
			"Invalid argument %s for command %s.\nNumber expected\n",
			endptr, function_name);
		return 0;
	}
	return 1;
}

/**
 * Loads user arguments for a command with cmd_num into user_args
 * @param char **argv - passing command line arguments
 * @param int *user_args - pointer to user_args structure to load information
 * @param char *str - argument for cset
 * @return int 1 if everything went well, 0 otherwise
 */
int load_command_user_args(char **argv, user_args_t *user_args)
{
	int cmd_num = user_args->cmd_num;
	int i, no_args = commands_s[cmd_num].no_args;
	char function_name[LENGTH_NAME];
	user_args->dash1 = user_args->dash2 = 0;
	for (i = 0; i < no_args; i++)
	{
		if (*++argv)
		{
			if (valid_str_arg(*argv, cmd_num, i, user_args))
			{
				if (strlen(*argv) >= MAX_CELL)
				{
					fprintf(stderr, "Argument %s too long.\n", *argv);
					return 0;
				}
				strcpy(user_args->str_arg, *argv);
			}
			else if (!valid_num_arg(*argv, i, user_args))
				return 0;
		}
		else
		{
			strcpy(function_name, commands_s[cmd_num].name);
			fprintf(stderr, "Invalid amount of arguments for command %s\n",
					function_name);
			return 0;
		}
	}
	return 1;
}

int main(int argc, char **argv)
{
	char *delim = " ";

	user_args_t user_args[argc];
	int arg_i = 0;
	cmd_types_t cmd_types = { 0, 0, 0 };

	while (*++argv)
	{
		if (strcmp(*argv, "-d") == 0)
		{
			if (*++argv)
				delim = *argv;
			else
			{
				fprintf(stderr, "Delimiter not given!\n");
				return 1;
			}
		}
		else if (is_command(*argv, &user_args[arg_i]))
		{
			if (load_command_user_args(argv, &user_args[arg_i]))
			{
				int cmd_num = user_args[arg_i++].cmd_num;
				// move argv pointer by number of arguments, if it suceeded this
				// many arguments were already parsed, otherwise it failed and
				// wont even get here
				argv += commands_s[cmd_num].no_args;
				if (IS_MOD(cmd_num))
					cmd_types.mod += 1;
				if (IS_DATA(cmd_num))
					cmd_types.data += 1;
				if (IS_SELECTION(cmd_num))
					cmd_types.selection += 1;
			}
			else
				return EXIT_FAILURE;
		}
		else
		{
			fprintf(stderr, "Unexpected argument %s\n", *argv);
			return EXIT_FAILURE;
		}
	}

	if(!handle_commands(cmd_types, user_args, arg_i, delim))
		return EXIT_FAILURE;
	else
		return EXIT_SUCCESS;
}
