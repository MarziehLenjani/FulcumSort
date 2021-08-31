#include <stdio.h>
#include <iostream>
#include "configAndStats.hpp"
#include "physicalComponent.hpp"
#include "stackedMemory.hpp"
#include "statConfigNamesAndTypes.hpp"
#include "dataPartitioning.hpp"
#include "test.hpp"
#include "computSubarray.hpp"
#include <math.h>
#include <cassert>
#include "omp.h"

using namespace std;

//#ifdef USE_THE_MAIN_MAIN
int main(int argc, char **argv)
{
    long long int c=0;
	configAndStats confObj;
	confObj.parseOptions(argc, argv);
	tests tstObj;

	//----------------------building a 3d stack component
	stackedMemory stackedMemoryObj((ID_TYPE)0,&confObj, NULL, NULL, NULL);
	//-----------------------building dataPartitioning object to its APIs
	dataPartitioning dataPartitioningObj(&stackedMemoryObj,&confObj);

	//----------------
	CONF_TEST_NUMBER_TYPE testNumber=confObj.getConfig< CONF_TEST_NUMBER_TYPE>(std::string(CONF_TEST_NUMBER_NAME));
	std::cout<<"testNumber is "<<testNumber<<std::endl;

	if(testNumber==(CONF_TEST_NUMBER_TYPE)0){
		int maxNumClockCycle=100000;

		int minRand = 0;
		int maxRand = 0x8FFFFFFF;
		int seed = 56;

		bool writeMetadat=true;

		// Subarray is organized as:
		// [metadata]   [histogram]   [read_array]   [write_array]
		// Read and write array is swapped every iteration

		// [metadata] is organized as:
		// [hist_start_addr] [hist_end_addr] [read_start_addr] [read_end_addr] [write_start_addr] [write_end_addr] ... others

		cout << "Total compute subarrays: " << stackedMemoryObj.totNumComputeSubarray << endl;
		u64 numOfDataElems = stackedMemoryObj.totNumComputeSubarray * 100 + 7; // to test unequal partitions
		cout << "Data elements: " << numOfDataElems << endl;
		cout << endl << endl;

		// Initialize known meta data
		histStartAddr = G_NUM_BYTES_IN_ROW;		//start of 2nd row
		histEndAddr = histStartAddr + G_NUM_HIST_ELEMS * sizeof(HIST_ELEM_TYPE);

		//read array starts right after the histogram's reserved space
		//read end address is filled by partition function
		u64 readStartAddr = ceil(histEndAddr * 1.0 / G_NUM_BYTES_IN_ROW) * G_NUM_BYTES_IN_ROW;

		//check the alignment of readStartAddr (should be aligned to a row)
		u64 rowMask = G_NUM_BYTES_IN_ROW - 1;
		assert((readStartAddr & rowMask) == 0UL);

		u64 maxElemsPerSubarray = numOfDataElems / stackedMemoryObj.totNumComputeSubarray;
		if(numOfDataElems % stackedMemoryObj.totNumComputeSubarray){
			maxElemsPerSubarray++;
		}
		reservedBytesForReadWriteArray = maxElemsPerSubarray * sizeof(KEY_TYPE);
		//Reserved bytes should be a power of two, and should be aligned to a row.
		//This is to make sure that placement location can be calculated with bitwise operations rather than division/mod.
		reservedBytesForReadWriteArray = getNextPow2(reservedBytesForReadWriteArray, G_NUM_BYTES_IN_ROW);

		u64 writeStartAddr = readStartAddr + reservedBytesForReadWriteArray;

		//check the alignment of writeStartAddr (should be aligned to a row)
		assert((writeStartAddr & rowMask) == 0UL);

		//check if the end of write array fits within the subarray
		assert((writeStartAddr + reservedBytesForReadWriteArray) <=
				confObj.getConfig<CONF_SIZE_OF_EACH_SUBARRAY_IN_BYTE_TYPE>(CONF_SIZE_OF_EACH_SUBARRAY_IN_BYTE_NAME));

		for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
			sub->writeData<LOCAL_ADDRESS_TYPE>(G_ADDR_OF_HIST_START_ADDR, histStartAddr);
			sub->writeData<LOCAL_ADDRESS_TYPE>(G_ADDR_OF_HIST_END_ADDR, histEndAddr);
			sub->writeData<LOCAL_ADDRESS_TYPE>(G_ADDR_OF_READ_START_ADDR, readStartAddr);
			sub->writeData<LOCAL_ADDRESS_TYPE>(G_ADDR_OF_WRITE_START_ADDR, writeStartAddr);
		}

		// Initialize Subarrays selfindexes
		stackedMemoryObj.initializeSubarraysSelfindexes();

		ERROR_RETURN_TYPE ret = dataPartitioningObj.generateRandomlyAndPartitionEquallyAmongAllComputeSubArray (minRand, maxRand, seed, readStartAddr, writeMetadat, G_ADDR_OF_READ_START_ADDR, G_ADDR_OF_READ_END_ADDR, numOfDataElems);

//		cout << "Initial: " << endl;
//		for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
//			sub->printReadElements();
//		}
//		cout << endl << endl;

		for(u64 startBit = 0; startBit < G_KEY_BITS; startBit += G_RADIX_BITS){
			u64 endBit = min(G_KEY_BITS, startBit + G_RADIX_BITS);
			cout << "Processing bits [" << endBit - 1 << " : " << startBit << "]" << endl;


			//----------------------------------------Local Sort----------------------------------------------
			radixSortShift = startBit;
			radixSortMask = radixSortMask = (1UL << endBit) - (1UL << radixSortShift);

			// Only approximately model local sort time
			//#pragma omp parallel for
			for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
				sub->runLocalRadixSort();
				//sub->printReadElements();
			}
			simCycles += G_LOCAL_SORT_INIT_CYCLES + (reservedBytesForReadWriteArray / sizeof(KEY_TYPE)) * (endBit - startBit) * G_LOCAL_SORT_CYCLES_PER_ELEM_PER_BIT;
//			printSimCycle("\tFinished local sort");

			// Initialization that are needed only once per radix bits
			for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
				sub->initPerRadix(startBit, endBit);
			}


			//----------------------------------------Range Iteration-----------------------------------------
			for(KEY_TYPE rangeStart = 0; rangeStart < (1UL << G_RADIX_BITS); rangeStart += G_NUM_HIST_ELEMS){
				rangeEnd = rangeStart + G_NUM_HIST_ELEMS;

				cout << "\tRange [" << rangeStart << " : " << rangeEnd << "]" << endl;

				//----------------------------------- Local Histogram ----------------------------------------
				for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
					sub->initLocalHistPerRange();
				}

				numOfProcessedSubarrays = 0;
				while(numOfProcessedSubarrays != stackedMemoryObj.totNumComputeSubarray){
					//#pragma omp parallel for
					for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
						sub->runLocalHistOneCycle();
					}
					simCycles++;
				}
				printSimCycle("\t\tFinished building local histogram");
				//for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
				//	sub->printHist();
				//}


				//----------------------------------- Histogram Reduction -------------------------------------
				for(u64 i = 1; i < stackedMemoryObj.totNumComputeSubarray; i++){
					HIST_ELEM_TYPE* currSubHist = (HIST_ELEM_TYPE*)(stackedMemoryObj.computSubarrayVector[i]->memoryArrayObj->data + histStartAddr);
					HIST_ELEM_TYPE* prevSubHist = (HIST_ELEM_TYPE*)(stackedMemoryObj.computSubarrayVector[i-1]->memoryArrayObj->data + histStartAddr);

					for(u64 j = 0; j < G_NUM_HIST_ELEMS; j++){
						currSubHist[j] += prevSubHist[j];
					}
				}
				//Approximate the timing of histogram reduction. Can be done because the accesses are sequential.
				simCycles += (G_NUM_HIST_ELEMS + stackedMemoryObj.totNumComputeSubarray) * G_REDUCTION_PER_HIST_ELEM_CYCLES;

				//Calculate prefix sum of the last subarray
				HIST_ELEM_TYPE prefixSum[G_NUM_HIST_ELEMS];
				prefixSum[0] = lastIdx;
				HIST_ELEM_TYPE* lastSubHist = (HIST_ELEM_TYPE*)(stackedMemoryObj.computSubarrayVector[stackedMemoryObj.totNumComputeSubarray-1]->memoryArrayObj->data + histStartAddr);
				for(u64 i = 1; i < G_NUM_HIST_ELEMS; i++){
					prefixSum[i] = prefixSum[i-1] + lastSubHist[i-1];
				}
				lastIdx = prefixSum[G_NUM_HIST_ELEMS - 1] + lastSubHist[G_NUM_HIST_ELEMS - 1];

				//Add offset to get final location
				for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
					HIST_ELEM_TYPE* histArray = (HIST_ELEM_TYPE*)(sub->memoryArrayObj->data + histStartAddr);
					for(u64 i = 0; i < G_NUM_HIST_ELEMS; i++){
						histArray[i] += prefixSum[i];
					}
				}
				//Approximate the timing of offset calculation (we can also ignore this part as the time required would be really small)
				simCycles += G_NUM_HIST_ELEMS * G_OFFSET_PER_HIST_ELEM_CYCLES;
				printSimCycle("\t\tFinished reducing histogram");


				//--------------------------------------- Placement ------------------------------------------
				for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
					sub->initPlacementPerRange();
				}

				numOfProcessedSubarrays = 0;
				numOfInFlightPackets = 0;
				while((numOfProcessedSubarrays != stackedMemoryObj.totNumComputeSubarray) || (numOfInFlightPackets != 0)){
					//#pragma omp parallel for
					for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
						sub->runPlacementOneCycle();
					}
					simCycles++;
				}
				printSimCycle("\t\tFinished Placement");
			}

			//Prepare for next radix
			//swap src and dst arrays
			for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
				sub->swapReadWriteArray();
			}
			lastIdx = 0;

//			for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
//				sub->printReadElements();
//			}

			printSimCycle("\tdone");
			cout << endl;
		}

//		cout << "After sort: " << endl;
//		for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
//			sub->printReadElements();
//		}

		printSimCycle("FINISHED!");

		//Check validity of output
		//1. Merge output of subarrays
		vector<KEY_TYPE> outData;
		outData.reserve(numOfDataElems);
		for(computSubarray* sub : stackedMemoryObj.computSubarrayVector){
			LOCAL_ADDRESS_TYPE startAddr = sub->memoryArrayObj->readLocalAddr(G_ADDR_OF_READ_START_ADDR);
			LOCAL_ADDRESS_TYPE endAddr = sub->memoryArrayObj->readLocalAddr(G_ADDR_OF_READ_END_ADDR);
			while(startAddr < endAddr){
				outData.push_back(sub->memoryArrayObj->readKey(startAddr));
				startAddr += sizeof(KEY_TYPE);
			}
		}

		assert(outData.size() == numOfDataElems);

		sort(dataArray, dataArray + numOfDataElems);
		for(u64 i = 0; i < numOfDataElems; i++){
			if(dataArray[i] != outData[i]){
				cout << "[FAIL at " << i << "] expected: " << dataArray[i] << "       actual: " << outData[i] << endl;
				return -1;
			}
		}
		cout << "[VALIDITY CHECK PASSED]" << endl;
	}
//	else if(testNumber==(CONF_TEST_NUMBER_TYPE)1){
//		int maxNumClockCycle=1000; //temp required before implementation ends
//
//		//step 1: Filling Subarrays with random data /dataset data using APIS in dataTransfer.hpp
//
//		int minRand=0;
//		int maxRand=100;
//		int seed=56;
//
//		bool writeMetadat=true;
//		//The first row of the Subarray always stores the metadata,
//		LOCAL_ADDRESS_TYPE AddressOfTheStartAddress= 0; //so the start address of the inputData should be written in address zero
//		LOCAL_ADDRESS_TYPE AddressOfTheEndAdddress=4;   //the end address of the inputData should be written in address 4
//		LOCAL_ADDRESS_TYPE DataAddress= (LOCAL_ADDRESS_TYPE) 64; // the input data starts from address 64
//		LOCAL_ADDRESS_TYPE numOFDataElements =stackedMemoryObj.totNumComputeSubarray*10+7; // to test unequal partitions
//		std::cout<<"stackedMemoryObj.totNumComputeSubarray:"<<stackedMemoryObj.totNumComputeSubarray<<std::endl;
//		ERROR_RETURN_TYPE ret=dataPartitioningObj.generateRandomlyAndPartitionEquallyAmongAllComputeSubArray (minRand, maxRand, seed, DataAddress, writeMetadat, AddressOfTheStartAddress, AddressOfTheEndAdddress, numOFDataElements );
//
//		//step 2: Initialize Subarrays selfindexes
//		stackedMemoryObj.initializeSubarraysSelfindexes();
//
//		//step 3: Determine number of iteration that we need on data
//
//		FULCRU_WORD_TYPE numberOfIteration=ceil((sizeof(FULCRU_WORD_TYPE)*8.0)/log2 (stackedMemoryObj.totNumComputeSubarray)); //
//		FULCRU_WORD_TYPE prevNumberOfSubBuckets=0;
//		FULCRU_WORD_TYPE prevNumberOfBuckets=0;
//		FULCRU_WORD_TYPE numberOfShiftsForBucketIDExtraction=sizeof(FULCRU_WORD_TYPE)*8.0;
//		FULCRU_WORD_TYPE maxNumberOfSubBuckets=stackedMemoryObj.totNumComputeSubarray;
//
//		for (FULCRU_WORD_TYPE iter=0; iter<numberOfIteration; iter++ ){
//			//TODO: add the FULCRUM initilaization overhead for each step
//
//			FULCRU_WORD_TYPE numberOfBitsForBucketIDExtraction=std::min (log2 (stackedMemoryObj.totNumComputeSubarray),sizeof(FULCRU_WORD_TYPE)*8.0- (iter*log2 (stackedMemoryObj.totNumComputeSubarray)));
//			numberOfShiftsForBucketIDExtraction-=numberOfBitsForBucketIDExtraction;
//			//FULCRU_WORD_TYPE numberofBucketsInThisIteration=pow(numberOfBitsForBucketIDExtraction,2);
//			FULCRU_WORD_TYPE numberofBucketsInThisIteration=pow(2,numberOfBitsForBucketIDExtraction);
//			FULCRU_WORD_TYPE maskForBucketExtaction = log2(numberofBucketsInThisIteration) - (1 << numberOfShiftsForBucketIDExtraction); //TODO: make sure this is correct
//
//
//			//
//			long long int stepClokCycleCounter=0;
//			FULCRU_WORD_TYPE subBucketCounter;
//			if(iter==0){ // In the first iteration there is no subucket
//
//				stackedMemoryObj.setMaskForBucketIDExtraction( maskForBucketExtaction, numberOfShiftsForBucketIDExtraction);
//				stackedMemoryObj.openANewSubBucket();
//				subBucketCounter=0;
//
//				while(!stackedMemoryObj.checkIfProcessHasEnd() && stepClokCycleCounter<maxNumClockCycle ){
//					stackedMemoryObj.runOneSubClokCycle(); //
//
//					if(stackedMemoryObj.checkIfSubBlockLimitIsReached() & subBucketCounter++<maxNumberOfSubBuckets){ //TODO: do we need to make sure that we have exactly maxNumberOfSubBuckets subBuckets?
//
//						stackedMemoryObj.sealAllSubBuckets();
//						stackedMemoryObj.incrementSubarraysSelfindexes();
//						stackedMemoryObj.openANewSubBucket();
//						subBucketCounter++;
//						stepClokCycleCounter+=100; //TODO: replace 100 with clock cycle required for above tasks or model them in the runOneSubClockCycle
//
//					}else{
//						stepClokCycleCounter++;
//					}
//
//				}
//
//
//			}else{ // after the first iteration subBuckets have been formed.
//				//We need to process buckets of the previous iteration in serial but subBuckets can help paralleize the process
//				for (FULCRU_WORD_TYPE bucketCounter=0; bucketCounter<prevNumberOfBuckets ; bucketCounter++ ){
//
//
//				}
//
//			}
//			prevNumberOfSubBuckets=subBucketCounter;
//			prevNumberOfBuckets=numberofBucketsInThisIteration;
//
//
//
//			std::cout<<"stepClokCycleCounter"<< stepClokCycleCounter<<std::endl;
//			stepClokCycleCounter+=stepClokCycleCounter;
//		}
//	}
	else {
		bool testsuccess=tstObj.runTestByTestNumber(testNumber,&confObj, &stackedMemoryObj);
	}

}
//#endif
