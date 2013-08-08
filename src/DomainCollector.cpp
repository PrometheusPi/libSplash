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



#include <algorithm>
#include <cassert>
#include <math.h>

#include "basetypes/ColTypeDim.hpp"
#include "basetypes/ColTypeInt.hpp"

#include "DomainCollector.hpp"
#include "DCDataSet.hpp"

// enable (1)/ disable (0) for printing of debug information
#define DC_DEBUG 0

namespace DCollector
{

    DomainCollector::DomainCollector(uint32_t maxFileHandles) :
    SerialDataCollector(maxFileHandles)
    {
    }

    DomainCollector::~DomainCollector()
    {
    }

    bool DomainCollector::testIntersection(Domain& d1, Domain& d2)
    {
        Dimensions d1_start = d1.getStart();
        Dimensions d2_start = d2.getStart();
        Dimensions d1_end = d1.getEnd();
        Dimensions d2_end = d2.getEnd();

        return (d1_start[0] <= d2_end[0] && d1_end[0] >= d2_start[0] &&
                d1_start[1] <= d2_end[1] && d1_end[1] >= d2_start[1] &&
                d1_start[2] <= d2_end[2] && d1_end[2] >= d2_start[2]) ||
                (d2_start[0] <= d1_end[0] && d2_end[0] >= d1_start[0] &&
                d2_start[1] <= d1_end[1] && d2_end[1] >= d1_start[1] &&
                d2_start[2] <= d1_end[2] && d2_end[2] >= d1_start[2]);
    }

    size_t DomainCollector::getTotalElements(int32_t id,
            const char* name)
    throw (DCException)
    {
        if ((fileStatus != FST_MERGING) && (fileStatus != FST_READING))
            throw DCException("DomainCollector::getTotalElements: this access is not permitted");

        size_t total_elements = 0;
        Dimensions mpi_size;
        if (fileStatus == FST_MERGING)
            mpi_size.set(mpiSize);
        else
            mpi_size.set(1, 1, 1);

        for (size_t z = 0; z < mpi_size[2]; z++)
        {
            for (size_t y = 0; y < mpi_size[1]; y++)
            {
                for (size_t x = 0; x < mpi_size[0]; x++)
                {
                    Dimensions mpi_position(x, y, z);

                    Dimensions subdomain_elements;
                    readAttribute(id, name, DOMCOL_ATTR_ELEMENTS,
                            subdomain_elements.getPointer(), &mpi_position);

                    total_elements += subdomain_elements.getDimSize();
                }
            }
        }

        return total_elements;
    }

    Domain DomainCollector::getTotalDomain(int32_t id,
            const char* name)
    throw (DCException)
    {
        if ((fileStatus != FST_MERGING) && (fileStatus != FST_READING))
            throw DCException("DomainCollector::getTotalDomain: this access is not permitted");

        Dimensions total_size(1, 1, 1);
        Dimensions offset(0, 0, 0);
        Dimensions mpi_size;
        if (fileStatus == FST_MERGING)
            mpi_size.set(mpiSize);
        else
            mpi_size.set(1, 1, 1);

        for (size_t z = 0; z < mpi_size[2]; z++)
        {
            for (size_t y = 0; y < mpi_size[1]; y++)
            {
                for (size_t x = 0; x < mpi_size[0]; x++)
                {
                    Dimensions mpi_position(x, y, z);

                    Dimensions subdomain_size;
                    Dimensions subdomain_start;

                    readAttribute(id, name, DOMCOL_ATTR_SIZE,
                            subdomain_size.getPointer(), &mpi_position);
                    readAttribute(id, name, DOMCOL_ATTR_START,
                            subdomain_start.getPointer(), &mpi_position);

                    if (x == 0 && y == 0 && z == 0)
                        offset.set(subdomain_start);

                    for (int i = 0; i < 3; ++i)
                    {
                        if (subdomain_start[i] + subdomain_size[i] > total_size[i])
                            total_size[i] = subdomain_start[i] + subdomain_size[i];

                        if (subdomain_start[i] < offset[i])
                            offset[i] = subdomain_start[i];
                    }
                }
            }
        }

        Domain domain(offset, total_size - offset);
        return domain;
    }

    bool DomainCollector::readDomainInfoForRank(
            Dimensions mpiPosition,
            int32_t id,
            const char* name,
            Dimensions requestOffset,
            Dimensions requestSize,
            Domain &fileDomain)
    {
        Domain request_domain(requestOffset, requestSize);

        readAttribute(id, name, DOMCOL_ATTR_START,
                fileDomain.getStart().getPointer(), &mpiPosition);

        readAttribute(id, name, DOMCOL_ATTR_SIZE,
                fileDomain.getSize().getPointer(), &mpiPosition);

        return testIntersection(request_domain, fileDomain);
    }

    bool DomainCollector::readDomainDataForRank(
            DataContainer *dataContainer,
            DomDataClass *dataClass,
            Dimensions mpiPosition,
            int32_t id,
            const char* name,
            Dimensions requestOffset,
            Dimensions requestSize,
            bool lazyLoad)
    {
#if (DC_DEBUG == 1)
        std::cerr << "\nloading from mpi_position " << mpi_position.toString() << std::endl;
#endif

        bool readResult = false;
        Domain request_domain(requestOffset, requestSize);

        Domain client_domain;
        readAttribute(id, name, DOMCOL_ATTR_START,
                client_domain.getStart().getPointer(), &mpiPosition);

        readAttribute(id, name, DOMCOL_ATTR_SIZE,
                client_domain.getSize().getPointer(), &mpiPosition);

        Dimensions data_elements;
        readAttribute(id, name, DOMCOL_ATTR_ELEMENTS,
                data_elements.getPointer(), &mpiPosition);

        DomDataClass tmp_data_class = UndefinedType;
        readAttribute(id, name, DOMCOL_ATTR_CLASS, &tmp_data_class, &mpiPosition);

        if (tmp_data_class == GridType && data_elements != client_domain.getSize())
            throw DCException("DomainCollector::readDomain: Number of data elements must match domain size for Grid data.");

        if (*dataClass == UndefinedType)
        {
            *dataClass = tmp_data_class;
        } else
            if (tmp_data_class != *dataClass)
        {
            throw DCException("DomainCollector::readDomain: Data classes in files are inconsistent!");
        }

#if (DC_DEBUG == 1)
        std::cerr << "clientdom. = " << client_domain.toString() << std::endl;
        std::cerr << "requestdom. = " << request_domain.toString() << std::endl;
#endif

        // test on intersection and add new DomainData to the container if necessary
        if (testIntersection(request_domain, client_domain))
        {
            readResult = true;

            // Poly data has no internal grid structure, 
            // so the whole chunk has to be read and is added to the DataContainer.
            if (*dataClass == PolyType)
            {
#if (DC_DEBUG == 1)
                std::cerr << "dataclass = Poly" << std::endl;
#endif
                if (data_elements.getDimSize() > 0)
                {
                    std::stringstream group_id_name;
                    group_id_name << SDC_GROUP_DATA << "/" << id;
                    std::string group_id_string = group_id_name.str();

                    hid_t group_id = H5Gopen(handles.get(mpiPosition), group_id_string.c_str(), H5P_DEFAULT);
                    if (group_id < 0)
                        throw DCException("DomainCollector::readDomain: group not found");

                    size_t datatype_size = 0;
                    DCDataSet::DCDataType dc_datatype = DCDataSet::DCDT_UNKNOWN;

                    try
                    {
                        DCDataSet tmp_dataset(name);
                        tmp_dataset.open(group_id);

                        datatype_size = tmp_dataset.getDataTypeSize();
                        dc_datatype = tmp_dataset.getDCDataType();

                        tmp_dataset.close();
                    } catch (DCException e)
                    {
                        H5Gclose(group_id);
                        throw e;
                    }

                    H5Gclose(group_id);

                    DomainData *client_data = new DomainData(client_domain,
                            data_elements, datatype_size, dc_datatype);

                    if (lazyLoad)
                    {
                        client_data->setLoadingReference(*dataClass,
                                handles.get(mpiPosition), id, name,
                                data_elements,
                                Dimensions(0, 0, 0),
                                Dimensions(0, 0, 0),
                                Dimensions(0, 0, 0));
                    } else
                    {
                        Dimensions elements_read;
                        uint32_t src_rank = 0;
                        readInternal(handles.get(mpiPosition), id, name,
                                data_elements,
                                Dimensions(0, 0, 0),
                                Dimensions(0, 0, 0),
                                Dimensions(0, 0, 0),
                                elements_read,
                                src_rank,
                                client_data->getData());

#if (DC_DEBUG == 1)
                        std::cerr << elements_read.toString() << std::endl;
                        std::cerr << data_elements.toString() << std::endl;
#endif

                        if (!(elements_read == data_elements))
                            throw DCException("DomainCollector::readDomain: Sizes are not equal but should be (1).");
                    }

                    dataContainer->add(client_data);
                } else
                {
#if (DC_DEBUG == 1)
                    std::cerr << "skipping entry with 0 elements" << std::endl;
#endif
                }
            } else
                // For Grid data, only the subchunk is read into its target position
                // in the destination buffer.
                if (*dataClass == GridType)
            {
#if (DC_DEBUG == 1)
                std::cerr << "dataclass = Grid" << std::endl;
#endif

                // When the first intersection is found, the whole destination 
                // buffer is allocated and added to the container.
                if (dataContainer->getNumSubdomains() == 0)
                {
                    std::stringstream group_id_name;
                    group_id_name << SDC_GROUP_DATA << "/" << id;
                    std::string group_id_string = group_id_name.str();

                    hid_t group_id = H5Gopen(handles.get(mpiPosition), group_id_string.c_str(), H5P_DEFAULT);
                    if (group_id < 0)
                        throw DCException("DomainCollector::readDomain: group not found");

                    size_t datatype_size = 0;
                    DCDataSet::DCDataType dc_datatype = DCDataSet::DCDT_UNKNOWN;

                    try
                    {
                        DCDataSet tmp_dataset(name);
                        tmp_dataset.open(group_id);

                        datatype_size = tmp_dataset.getDataTypeSize();
                        dc_datatype = tmp_dataset.getDCDataType();

                        tmp_dataset.close();
                    } catch (DCException e)
                    {
                        H5Gclose(group_id);
                        throw e;
                    }

                    H5Gclose(group_id);

                    DomainData *target_data = new DomainData(
                            request_domain, request_domain.getSize(),
                            datatype_size, dc_datatype);

                    dataContainer->add(target_data);
                }

                // Compute the offsets and sizes for reading and
                // writing this intersection.
                Dimensions dst_offset(0, 0, 0);
                Dimensions src_size(1, 1, 1);
                Dimensions src_offset(0, 0, 0);

                Dimensions client_start = client_domain.getStart();
                Dimensions client_size = client_domain.getSize();

                size_t rank = getRank(handles.get(mpiPosition), id, name);

                for (uint32_t i = 0; i < rank; ++i)
                {
                    dst_offset[i] = std::max((int64_t) client_domain.getStart()[i] - (int64_t) requestOffset[i], 0L);

                    if (requestOffset[i] <= client_start[i])
                    {
                        src_offset[i] = 0;

                        if (requestOffset[i] + requestSize[i] >= client_start[i] + client_size[i])
                            src_size[i] = client_size[i];
                        else
                            src_size[i] = requestOffset[i] + requestSize[i] - client_start[i];
                    } else
                    {
                        src_offset[i] = requestOffset[i] - client_start[i];

                        if (requestOffset[i] + requestSize[i] >= client_start[i] + client_size[i])
                            src_size[i] = client_size[i] - src_offset[i];
                        else
                            src_size[i] = requestOffset[i] + requestSize[i] -
                                (client_start[i] + src_offset[i]);
                    }
                }

#if (DC_DEBUG == 1)
                std::cerr << "client_domain.getSize() = " <<
                        client_domain.getSize().toString() << std::endl;
                std::cerr << "data_elements = " <<
                        data_elements.toString() << std::endl;
                std::cerr << "dst_offset = " << dst_offset.toString() << std::endl;
                std::cerr << "src_size = " << src_size.toString() << std::endl;
                std::cerr << "src_offset = " << src_offset.toString() << std::endl;

                assert(src_size[0] <= request_domain.getSize()[0]);
                assert(src_size[1] <= request_domain.getSize()[1]);
                assert(src_size[2] <= request_domain.getSize()[2]);
#endif

                // read intersecting partition into destination buffer
                Dimensions elements_read(0, 0, 0);
                uint32_t src_rank = 0;
                readInternal(handles.get(mpiPosition), id, name,
                        dataContainer->getIndex(0)->getSize(),
                        dst_offset,
                        src_size,
                        src_offset,
                        elements_read,
                        src_rank,
                        dataContainer->getIndex(0)->getData());

#if (DC_DEBUG == 1)
                std::cerr << "elements_read = " << elements_read.toString() << std::endl;
#endif

                if (!(elements_read == src_size))
                    throw DCException("DomainCollector::readDomain: Sizes are not equal but should be (2).");
            }
        } else
        {
#if (DC_DEBUG == 1)
            std::cerr << "no loading from this mpi position required" << std::endl;
#endif
        }

        return readResult;
    }

    DataContainer *DomainCollector::readDomain(int32_t id,
            const char* name,
            Dimensions requestOffset,
            Dimensions requestSize,
            DomDataClass* dataClass,
            bool lazyLoad)
    throw (DCException)
    {
        if ((fileStatus != FST_MERGING) && (fileStatus != FST_READING))
            throw DCException("DomainCollector::readDomain: this access is not permitted");

        DataContainer *data_container = new DataContainer();

#if (DC_DEBUG == 1)
        std::cerr << "requestOffset = " << requestOffset.toString() << std::endl;
        std::cerr << "requestSize = " << requestSize.toString() << std::endl;
#endif

        DomDataClass data_class = UndefinedType;
        Dimensions mpi_position(0, 0, 0);

        readDomainDataForRank(data_container,
                &data_class,
                mpi_position,
                id,
                name,
                requestOffset,
                requestSize,
                lazyLoad);

        if (dataClass != NULL)
            *dataClass = data_class;

        return data_container;
    }

    void DomainCollector::readDomainLazy(DomainData *domainData)
    throw (DCException)
    {
        if (domainData == NULL)
        {
            throw DCException("DomainCollector::readDomain: Invalid parameter, DomainData must not be NULL");
        }

        DomainH5Ref *loadingRef = domainData->getLoadingReference();
        if (loadingRef == NULL)
        {
            throw DCException("DomainCollector::readDomain: This DomainData does not allow lazy loading");
        }

        if (loadingRef->dataClass == UndefinedType)
        {
            throw DCException("DomainCollector::readDomain: DomainData has invalid data class");
        }

        if (loadingRef->dataClass == PolyType)
        {
            Dimensions elements_read;
            uint32_t src_rank = 0;
            readInternal(loadingRef->handle,
                    loadingRef->id,
                    loadingRef->name.c_str(),
                    loadingRef->dstBuffer,
                    loadingRef->dstOffset,
                    loadingRef->srcSize,
                    loadingRef->srcOffset,
                    elements_read,
                    src_rank,
                    domainData->getData());

#if (DC_DEBUG == 1)
            std::cerr << elements_read.toString() << std::endl;
            std::cerr << loadingRef->dataElements.toString() << std::endl;
#endif

            if (!(elements_read == loadingRef->dstBuffer))
                throw DCException("DomainCollector::readDomain: Sizes are not equal but should be (1).");

        } else
        {
            throw DCException("DomainCollector::readDomain: data class not supported");
        }
    }

    void DomainCollector::writeDomain(int32_t id,
            const CollectionType& type,
            uint32_t rank,
            const Dimensions srcData,
            const char* name,
            const Dimensions domainOffset,
            const Dimensions domainSize,
            DomDataClass dataClass,
            const void* data)
    throw (DCException)
    {

        writeDomain(id, type, rank, srcData, Dimensions(1, 1, 1), srcData,
                Dimensions(0, 0, 0), name, domainOffset, domainSize, dataClass, data);
    }

    void DomainCollector::writeDomain(int32_t id,
            const CollectionType& type,
            uint32_t rank,
            const Dimensions srcBuffer,
            const Dimensions srcData,
            const Dimensions srcOffset,
            const char* name,
            const Dimensions domainOffset,
            const Dimensions domainSize,
            DomDataClass dataClass,
            const void* data)
    throw (DCException)
    {

        writeDomain(id, type, rank, srcBuffer, Dimensions(1, 1, 1), srcData, srcOffset,
                name, domainOffset, domainSize, dataClass, data);
    }

    void DomainCollector::writeDomain(int32_t id,
            const CollectionType& type,
            uint32_t rank,
            const Dimensions srcBuffer,
            const Dimensions srcStride,
            const Dimensions srcData,
            const Dimensions srcOffset,
            const char* name,
            const Dimensions domainOffset,
            const Dimensions domainSize,
            DomDataClass dataClass,
            const void* data)
    throw (DCException)
    {
        ColTypeDim dim_t;
        ColTypeInt int_t;

        write(id, type, rank, srcBuffer, srcStride, srcData, srcOffset, name, data);

        writeAttribute(id, int_t, name, DOMCOL_ATTR_CLASS, &dataClass);
        writeAttribute(id, dim_t, name, DOMCOL_ATTR_SIZE, domainSize.getPointer());
        writeAttribute(id, dim_t, name, DOMCOL_ATTR_START, domainOffset.getPointer());
        writeAttribute(id, dim_t, name, DOMCOL_ATTR_ELEMENTS, srcData.getPointer());
    }

    void DomainCollector::appendDomain(int32_t id,
            const CollectionType& type,
            uint32_t count,
            const char* name,
            const Dimensions domainOffset,
            const Dimensions domainSize,
            const void* data)
    throw (DCException)
    {
        appendDomain(id, type, count, 0, 1, name, domainOffset, domainSize, data);
    }

    void DomainCollector::appendDomain(int32_t id,
            const CollectionType& type,
            uint32_t count,
            uint32_t offset,
            uint32_t striding,
            const char* name,
            const Dimensions domainOffset,
            const Dimensions domainSize,
            const void* data)
    throw (DCException)
    {
        ColTypeDim dim_t;
        ColTypeInt int_t;
        DomDataClass data_class = PolyType;
        Dimensions elements(1, 1, 1);

        // temporarly change file access status to allow read access
        FileStatusType old_file_status = fileStatus;
        fileStatus = FST_READING;

        // try to get the number of elements already written, if any.
        try
        {
            readAttribute(id, name, DOMCOL_ATTR_ELEMENTS, elements.getPointer(), NULL);
        } catch (DCException expected_exception)
        {
            // nothing to do here but to make sure elements is set correctly
            elements.set(0, 1, 1);
        }

        fileStatus = old_file_status;

        elements[0] = elements[0] + count;

        append(id, type, count, offset, striding, name, data);

        writeAttribute(id, int_t, name, DOMCOL_ATTR_CLASS, &data_class);
        writeAttribute(id, dim_t, name, DOMCOL_ATTR_SIZE, domainSize.getPointer());
        writeAttribute(id, dim_t, name, DOMCOL_ATTR_START, domainOffset.getPointer());
        writeAttribute(id, dim_t, name, DOMCOL_ATTR_ELEMENTS, elements.getPointer());
    }
}
