/******************************************************************************
* Copyright (c) 2011, Michael P. Gerlek (mpg@flaxen.com)
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following
* conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in
*       the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of Hobu, Inc. or Flaxen Geo Consulting nor the
*       names of its contributors may be used to endorse or promote
*       products derived from this software without specific prior
*       written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
* OF SUCH DAMAGE.
****************************************************************************/

#include <cassert>
#include <iostream>

#include "libpc/PointLayout.hpp"

using std::cout;
using std::endl;
using std::string;

namespace libpc
{


PointLayout::PointLayout()
    : m_numBytes(0)
{
    return;
}


PointLayout::PointLayout(const PointLayout& other)
{
    m_numBytes = other.m_numBytes;
    m_fields = other.m_fields;
    return;
}


PointLayout& PointLayout::operator=(const PointLayout& other)
{
    if (this != &other)
    {
        m_numBytes = other.m_numBytes;
        m_fields = other.m_fields;
    }

    return *this;
}


bool PointLayout::operator==(const PointLayout & other) const
{
    if (m_numBytes != other.m_numBytes) return false;
    if (m_fields != other.m_fields) return false;

    // we don't need to check m_fieldIndex_X, etc, because those fields just
    // replicate the data in m_fields

    return true;
}


std::size_t PointLayout::addField(const Field& fieldParam)
{
    Field myField(fieldParam);

    const Field::DataItem item = myField.getItem();

    assert(!hasField(item));

    const std::size_t offset = m_numBytes;
    myField.setOffset(offset);
    m_numBytes += myField.getNumBytes();

    m_fields.push_back(myField);

    const std::size_t index = (boost::uint32_t)m_fields.size() - 1;

    return index;
}


std::size_t PointLayout::getSizeInBytes() const
{
    return m_numBytes;
}


std::size_t PointLayout::getNumFields() const
{
    return m_fields.size();
}


bool PointLayout::findFieldIndex(Field::DataItem item, std::size_t& ret) const
{
    for (size_t index=0; index<m_fields.size(); index++)
    {
        const Field& field = m_fields[index];
        if (field.getItem() == item)
        {
            ret = index;
            return true;
        }
    }

    return false;
}


bool PointLayout::hasField(Field::DataItem item) const
{
    for (size_t index=0; index<m_fields.size(); index++)
    {
        const Field& field = m_fields[index];
        if (field.getItem() == item)
        {
            return true;
        }
    }

    return false;
}


void PointLayout::dump(string indent) const
{
    cout << indent << "PointLayout:" << endl;

    for (size_t i=0; i<m_fields.size(); i++)
    {
        cout << indent << indent;
        m_fields[i].dump();
        cout << endl;
    }
}

} // namespace libpc
