#include "walkers.hpp"
#include "debug.hpp"

/*
#include <iostream>
#include "nlohmann/json.hpp"
#include <stdlib.h>

#define NUM_OF_COLUMNS	5	

class walker{
	public:
		// constructor for the walker
		walker(void) {
			unsigned int i =0;	// TODO remove
			last_accessed_column_index = 0; 	// 0--> indicates that the walker was untouched
			for(i=0; i<NUM_OF_COLUMNS; i++) {
				colData[i] = i;
			}

			isWalker1 = true;
			isWalker2 = false;
			isWalker3 = false;
		}

		// return the latest column index being accessed
		unsigned short lastAccessedColIndex(void) {
			return last_accessed_column_index;
		}

		// returns the data present in that column of the walker
		unsigned int columnData(unsigned short num) {
			if(last_accessed_column_index++ != num)
				debug_printf("[DEBUG] Appears like you are jumping in accessing 1 column\n");

			if(num < NUM_OF_COLUMNS)
				return colData[num];
			else {
				debug_printf("[DEBUG] rror in retrieving the values \n");
				return 0;	// TODO can we turn this into success/error type of returns?
			}

		}

		//
		// function used to write data into the columns of the walker
		// args - row number, columnar data
		// return type - 0-> success 1->error
		//
		unsigned char writeColumnData(unsigned int input_column_data[NUM_OF_COLUMNS]) {
			for(int i=0; i<NUM_OF_COLUMNS; i++) {
				colData[i] = input_column_data[i];
			}
			return 0;
			// TODO what are the error cases here?
		}

		//
		//print all the column data of this walker
		//
		unsigned char print_column_data(void) {
			for(int i=0; i<NUM_OF_COLUMNS; i++) {
				debug_printf("Column %d = %d \n", i, colData[i]);
			}
			return 0;
			// TODO what are the error cases here?
		}
	private:
		unsigned int colData[NUM_OF_COLUMNS];
		unsigned short last_accessed_column_index;		// Stores the value of the last accessed column by the ALPU
									// Suppose 1024 columns are their, in first cycle col1 and col2  get processed, thus current_column_index=2 at the end of first cycle
									// Also current_column_index = 1:NUM_OF_COLUMNS (Column 0 is not available for simplicity)
									// if lastAccessedColIndex = 0 then, it appears like the walker was untouched

		// Since each walker has directional (left-right) restrictions, we need this info to conform to these rules
		bool isWalker1;
		bool isWalker2;
		bool isWalker3;

};
*/
#ifdef MAIN_IN_WALKER
int main(int argc, char* argv[])
{
	unsigned int value;
	unsigned int columnar_data[NUM_OF_COLUMNS];
	walker job1[3];

	for(int i=2; i<argc; i++) {
		columnar_data[i-2] = atoi(argv[i]);
	}

	for (int i=0;i<1; i++) {
		debug_printf("Iteration %d \n", i);

	debug_printf(" .... Writing data into the walker %d ....\n",i);
	job1[i].writeColumnData(columnar_data);

	debug_printf("The last accessed column was %d \n", job1[i].lastAccessedColIndex());
	value = job1[i].read_columnData(2);
	debug_printf("Value present in the second column = %d \n", value);


	debug_printf(" .... Reading data back from the walker %d .... \n", i);
	job1[i].print_column_data();
	}


	return 0;

	//
	//
	//
	//:q
	//

}
#endif
