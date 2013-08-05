/**
 * Copyright 2013 Felix Schmitt
 *
 * This file is part of libSplash. 
 * 
 * libSplash is free software: you can redistribute it and/or modify 
 * it under the terms of of either the GNU General Public License or 
 * the GNU Lesser General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version. 
 * libSplash is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License and the GNU Lesser General Public License 
 * for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * and the GNU Lesser General Public License along with libSplash. 
 * If not, see <http://www.gnu.org/licenses/>. 
 */

#ifndef PARALLELDATACOLLECTOR_HPP
#define	PARALLELDATACOLLECTOR_HPP

#include <hdf5.h>
#include <mpi.h>
#include <sstream>
#include <string>
#include <iostream>
#include <stdexcept>

#include "IParallelDataCollector.hpp"

#include "DCException.hpp"
#include "sdc_defines.hpp"
#include "HandleMgr.hpp"

namespace DCollector
{

    class ParallelDataCollector : public IParallelDataCollector
    {
    private:
        /**
         * Set properties for file access property list.
         *
         * @param fileAccProperties reference to fileAccProperties to set parameters for
         */
        void setFileAccessParams(hid_t& fileAccProperties);

        /**
         * Constructs a filename from a base filename and the current id
         * such as baseFilename+id+.h5
         * 
         * @param id current iteration
         * @param baseFilename base filename for the new file
         * @return newly constructed filename including file extension
         */
        static std::string getFullFilename(uint32_t id, std::string baseFilename);

        /**
         * Internal function for formatting exception messages.
         * 
         * @param func name of the throwing function
         * @param msg exception message
         * @param info optional info text to be appended, e.g. the group name
         * @return formatted exception message string
         */
        static std::string getExceptionString(std::string func, std::string msg,
                const char *info = NULL);
        
        static void indexToPos(int index, Dimensions mpiSize, Dimensions &mpiPos);
    protected:

        /**
         * internal type to save file access mode
         */
        enum FileStatusType
        {
            FST_CLOSED, FST_WRITING, FST_READING, FST_CREATING
        };

        // internal MPI structures
        MPI_Comm mpiComm;
        MPI_Info mpiInfo;
        int mpiRank;
        int mpiSize;
        Dimensions mpiPos;
        Dimensions mpiTopology;

        // internal hdf5 file handles
        HandleMgr handles;

        // property list for hdf5 file access
        hid_t fileAccProperties;

        // current file access type
        FileStatusType fileStatus;

        // enable data compression
        bool enableCompression;

        static void fileCreateCallback(H5Handle handle);

        void openCreate(const char *filename,
                FileCreationAttr &attr) throw (DCException);

        void openRead(const char *filename,
                FileCreationAttr &attr) throw (DCException);

        void openWrite(const char *filename,
                FileCreationAttr &attr) throw (DCException);

        void writeDataSet(
                hid_t &group,
                const Dimensions globalSize,
                const Dimensions globalOffset,
                const CollectionType& datatype,
                uint32_t rank,
                const Dimensions srcBuffer,
                const Dimensions srcStride,
                const Dimensions srcData,
                const Dimensions srcOffset,
                const char* name,
                const void* data) throw (DCException);

    public:
        /**
         * Constructor
         */
        ParallelDataCollector(MPI_Comm comm, MPI_Info info, const Dimensions topology,
                int mpiRank, uint32_t maxFileHandles);

        /**
         * Destructor
         */
        virtual ~ParallelDataCollector();

        /**
         * {@link DataCollector#open}
         */
        void open(const char *filename,
                FileCreationAttr& attr) throw (DCException);

        /**
         * {@link DataCollector#close}
         */
        void close();

        /**
         * {@link DataCollector#getMaxID}
         */
        int32_t getMaxID();

        /**
         * {@link DataCollector#getMPISize}
         */
        void getMPISize(Dimensions& mpiSize);

        /**
         * {@link DataCollector#getEntryIDs}
         */
        void getEntryIDs(int32_t *ids, size_t *count) throw (DCException);

        /**
         * {@link DataCollector#getEntriesForID}
         */
        void getEntriesForID(int32_t id, DCEntry *entries, size_t *count) throw (DCException);

        /**
         * {@link IParallelDataCollector#write}
         */
        void write(int32_t id,
                const CollectionType& type,
                uint32_t rank,
                const Dimensions srcData,
                const char* name,
                const void* data) throw (DCException);

        /**
         * {@link IParallelDataCollector#write}
         */
        void write(int32_t id,
                const CollectionType& type,
                uint32_t rank,
                const Dimensions srcBuffer,
                const Dimensions srcData,
                const Dimensions srcOffset,
                const char* name,
                const void* data) throw (DCException);

        /**
         * {@link IParallelDataCollector#write}
         */
        void write(int32_t id,
                const CollectionType& type,
                uint32_t rank,
                const Dimensions srcBuffer,
                const Dimensions srcStride,
                const Dimensions srcData,
                const Dimensions srcOffset,
                const char* name,
                const void* data) throw (DCException);

        /**
         * {@link IParallelDataCollector#write}
         */
        void write(int32_t id,
                const Dimensions globalSize,
                const Dimensions globalOffset,
                const CollectionType& type,
                uint32_t rank,
                const Dimensions srcData,
                const char* name,
                const void* data);

        /**
         * {@link IParallelDataCollector#write}
         */
        void write(int32_t id,
                const Dimensions globalSize,
                const Dimensions globalOffset,
                const CollectionType& type,
                uint32_t rank,
                const Dimensions srcBuffer,
                const Dimensions srcData,
                const Dimensions srcOffset,
                const char* name,
                const void* data);

        /**
         * {@link IParallelDataCollector#write}
         */
        void write(int32_t id,
                const Dimensions globalSize,
                const Dimensions globalOffset,
                const CollectionType& type,
                uint32_t rank,
                const Dimensions srcBuffer,
                const Dimensions srcStride,
                const Dimensions srcData,
                const Dimensions srcOffset,
                const char* name,
                const void* data);

        /**
         * {@link DataCollector#append}
         */
        void append(int32_t id,
                const CollectionType& type,
                uint32_t count,
                const char *name,
                const void *data) throw (DCException);

        /**
         * {@link DataCollector#append}
         */
        void append(int32_t id,
                const CollectionType& type,
                uint32_t count,
                uint32_t offset,
                uint32_t striding,
                const char *name,
                const void *data) throw (DCException);

        /**
         * {@link DataCollector#remove}
         */
        void remove(int32_t id) throw (DCException);

        /**
         * {@link DataCollector#remove}
         */
        void remove(int32_t id,
                const char *name) throw (DCException);

        /**
         * {@link DataCollector#createReference}
         */
        void createReference(int32_t srcID,
                const char *srcName,
                const CollectionType& colType,
                int32_t dstID,
                const char *dstName,
                Dimensions count,
                Dimensions offset,
                Dimensions stride) throw (DCException);

        /**
         * {@link DataCollector#readGlobalAttribute}
         */
        void readGlobalAttribute(
                const char *name,
                void* data,
                Dimensions *mpiPosition = NULL) throw (DCException);

        /**
         * {@link DataCollector#writeGlobalAttribute}
         */
        void writeGlobalAttribute(const CollectionType& type,
                const char *name,
                const void* data) throw (DCException);

        /**
         * {@link DataCollector#readAttribute}
         */
        void readAttribute(int32_t id,
                const char *dataName,
                const char *attrName,
                void *data,
                Dimensions *mpiPosition = NULL) throw (DCException);

        /**
         * {@link DataCollector#writeAttribute}
         */
        void writeAttribute(int32_t id,
                const CollectionType& type,
                const char *dataName,
                const char *attrName,
                const void *data) throw (DCException);

        /**
         * {@link DataCollector#read}
         */
        void read(int32_t id,
                const CollectionType& type,
                const char* name,
                Dimensions &srcData,
                void* data) throw (DCException);
        /**
         * {@link DataCollector#read}
         */
        void read(int32_t id,
                const CollectionType& type,
                const char* name,
                Dimensions dstBuffer,
                Dimensions &srcData,
                Dimensions dstOffset,
                void* data) throw (DCException);
    private:

    };

}

#endif	/* PARALLELDATACOLLECTOR_HPP */
